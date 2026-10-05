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
 * @file write_sorted_pixels.slang
 * @brief Compute pass that writes the pixels of the source image in the order of the sorted radix sort keys.
 * @details Dispatch one 8x8 workgroup per 8x8 pixel tile. A pixel inside the contrast mask receives the source pixel of
 * the sorted key at the index stored for it by register_pixel_sort_keys, which belongs to the same column, so the masked
 * pixels of every column end up sorted by luminance with the darkest at the top. Other pixels keep their source color.
 */
#include "utils.slang"
#include "sort/radix_sort_utils.slang"
#include "pixel_sort/pixel_sort_utils.slang"

/** Descriptor indices of the resources used by the write pass. */
struct WriteSortedPixelsDescriptorIndices {
    /** Image whose pixels are sorted. */
    [WBE_DESCRIPTOR("sampled_image", "pixel_sort_source")] uint2 source;
    /** Radix sort key buffer holding the sorted keys. */
    [WBE_DESCRIPTOR("storage_buffer", "pixel_sort_keys", "read")] uint2 keys;
    /** The key buffer index of every pixel, from register_pixel_sort_keys. */
    [WBE_DESCRIPTOR("storage_image", "pixel_sort_indices", "read")] uint2 indices;
    /** Image receiving the sorted pixels; must have the extent of source. */
    [WBE_DESCRIPTOR("storage_image", "pixel_sort_destination", "write")] uint2 destination;
};

[[vk::push_constant]]
ConstantBuffer<WriteSortedPixelsDescriptorIndices> descriptor_indices;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    Texture2D<float4> source = ResourceDescriptorHeap[descriptor_indices.source.x];
    RWStructuredBuffer<uint> keys = ResourceDescriptorHeap[descriptor_indices.keys.x];
    RWTexture2D<uint> indices = ResourceDescriptorHeap[descriptor_indices.indices.x];
    RWTexture2D<float4> destination = ResourceDescriptorHeap[descriptor_indices.destination.x];
    uint2 extent;
    source.GetDimensions(extent.x, extent.y);
    uint2 pixel = p_dispatch_thread_id.xy;
    if (any(pixel >= extent)) {
        return;
    }
    uint index = indices[pixel];
    uint2 source_pixel = pixel;
    if (index != WBE_PIXEL_SORT_UNMASKED) {
        source_pixel.y = pixel_sort_key_row(keys[WBE_RADIX_SORT_DATA_START + index], extent);
    }
    destination[pixel] = float4(source.Load(int3(source_pixel, 0)).rgb, 1.0);
}
