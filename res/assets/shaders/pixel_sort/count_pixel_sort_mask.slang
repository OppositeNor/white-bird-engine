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
 * @file count_pixel_sort_mask.slang
 * @brief Compute pass that counts the pixels inside the contrast mask in every sorted line of the source image.
 * @details One invocation per line; dispatch ceil(width / 64) workgroups, or ceil(height / 64) for the horizontal
 * variant. The column offset buffer receives the line count at index 0, the mask count of line l at
 * WBE_PIXEL_SORT_COLUMN_OFFSETS_START + l.
 */
#include "utils.slang"
#include "pixel_sort/pixel_sort_utils.slang"

/** Descriptor indices of the resources used by the count pass. */
struct CountPixelSortMaskDescIndices {
    /** Image whose pixels are sorted. */
    [wbe::desc("sampled_image", "pixel_sort_source")] DescriptorHandle<Texture2D<float4>> source;
    /** The contrast range of the sorted pixels. */
    [wbe::desc("uniform_buffer", "pixel_sort_contrast")] DescriptorHandle<ConstantBuffer<PixelSortContrast>> contrast;
    /** Buffer receiving the mask count of every line. */
    [wbe::desc("storage_buffer", "pixel_sort_column_offsets", "write")] DescriptorHandle<RWStructuredBuffer<uint>> column_offsets;
};

[vk::push_constant]
ConstantBuffer<CountPixelSortMaskDescIndices> di;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    Texture2D<float4> source = di.source;
    ConstantBuffer<PixelSortContrast> contrast = di.contrast;
    RWStructuredBuffer<uint> column_offsets = di.column_offsets;
    uint2 extent;
    source.GetDimensions(extent.x, extent.y);
    uint2 line_extent = pixel_sort_line_coordinates(extent);
    uint line = p_dispatch_thread_id.x;
    if (line == 0) {
        column_offsets[0] = line_extent.x;
    }
    if (line >= line_extent.x) {
        return;
    }
    uint count = 0;
    for (uint position = 0; position < line_extent.y; ++position) {
        uint2 pixel = pixel_sort_line_pixel(uint2(line, position));
        if (pixel_sort_in_contrast(source.Load(int3(pixel, 0)).rgb, contrast)) {
            ++count;
        }
    }
    column_offsets[WBE_PIXEL_SORT_COLUMN_OFFSETS_START + line] = count;
}
