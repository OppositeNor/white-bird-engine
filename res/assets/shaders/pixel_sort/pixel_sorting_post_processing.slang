/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
/**
 * @file pixel_sorting_post_processing.slang
 * @brief Compute pass that applies general post-processing effects to the pixel-sorted image.
 * @details Dispatch one 16x16 workgroup per 16x16 pixel tile of the sorted image. Every invocation writes one output pixel,
 * and may read any pixel of the sorted image, so effects that sample neighbors, such as blurs, are allowed. The effects
 * belong in post_process(); it currently applies gamma correction.
 */
#include "utils.slang"

/** Parameters of the post-processing effects. */
struct PixelSortingPostProcessingParams {
    /** The exponent every color channel is raised to; 1 leaves the color unchanged. */
    float gamma;
};

/** Descriptor indices of the resources used by the post-processing pass. */
struct PixelSortingPostProcessingDescIndices {
    /** The pixel-sorted image, from write_sorted_pixels. */
    [wbe::desc("sampled_image", "pixel_sort_destination")] DescriptorHandle<Texture2D<float4>> sorted_image;
    /** Clamp-to-edge sampler for filtered reads of the sorted image. */
    [wbe::desc("sampler", "pixel_sort_sampler")] DescriptorHandle<SamplerState> sorted_sampler;
    /** The parameters of the post-processing effects. */
    [wbe::desc("uniform_buffer", "pixel_sorting_post_processing_params")] DescriptorHandle<ConstantBuffer<PixelSortingPostProcessingParams>> params;
    /** Image receiving the post-processed pixels; must have the extent of sorted_image. */
    [wbe::desc("storage_image", "pixel_sorting_post_processing_output", "write")] DescriptorHandle<RWTexture2D<float4>> output;
};

[vk::push_constant]
ConstantBuffer<PixelSortingPostProcessingDescIndices> di;

/**
 * @brief Compute the post-processed color of one pixel.
 * @param p_sorted_image The pixel-sorted image.
 * @param p_sorted_sampler Clamp-to-edge sampler for filtered reads of p_sorted_image.
 * @param p_pixel The pixel to compute; inside p_extent.
 * @param p_extent The extent of p_sorted_image.
 * @param p_params The parameters of the post-processing effects.
 * @return The color of p_pixel in the output image.
 */
float4 post_process(Texture2D<float4> p_sorted_image,
    SamplerState p_sorted_sampler,
    uint2 p_pixel,
    uint2 p_extent,
    PixelSortingPostProcessingParams p_params) {
    float3 color = p_sorted_image.Load(int3(p_pixel, 0)).rgb;
    color = pow(max(color, 0.0), p_params.gamma);
    return float4(color, 1.0);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    Texture2D<float4> sorted_image = di.sorted_image;
    SamplerState sorted_sampler = di.sorted_sampler;
    ConstantBuffer<PixelSortingPostProcessingParams> params = di.params;
    RWTexture2D<float4> output = di.output;
    uint2 extent;
    sorted_image.GetDimensions(extent.x, extent.y);
    uint2 pixel = p_dispatch_thread_id.xy;
    if (any(pixel >= extent)) {
        return;
    }
    output[pixel] = post_process(sorted_image, sorted_sampler, pixel, extent, params);
}
