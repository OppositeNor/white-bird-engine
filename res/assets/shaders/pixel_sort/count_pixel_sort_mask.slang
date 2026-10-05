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
 * @brief Compute pass that counts the pixels inside the contrast mask in every column of the source image.
 * @details One invocation per column; dispatch ceil(width / 64) workgroups. The column offset buffer receives the column
 * count at index 0, the mask count of column c at WBE_PIXEL_SORT_COLUMN_OFFSETS_START + c, and the zero element the
 * prefix sum reads after the counts.
 */
#include "utils.slang"
#include "pixel_sort/pixel_sort_utils.slang"

/** Descriptor indices of the resources used by the count pass. */
struct CountPixelSortMaskDescriptorIndices {
    /** Image whose pixels are sorted. */
    [WBE_DESCRIPTOR("sampled_image", "pixel_sort_source")] uint2 source;
    /** The contrast range of the sorted pixels. */
    [WBE_DESCRIPTOR("uniform_buffer", "pixel_sort_contrast")] uint2 contrast;
    /** Buffer receiving the mask count of every column. */
    [WBE_DESCRIPTOR("storage_buffer", "pixel_sort_column_offsets", "write")] uint2 column_offsets;
};

[[vk::push_constant]]
ConstantBuffer<CountPixelSortMaskDescriptorIndices> descriptor_indices;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    Texture2D<float4> source = ResourceDescriptorHeap[descriptor_indices.source.x];
    ConstantBuffer<PixelSortContrast> contrast = ResourceDescriptorHeap[descriptor_indices.contrast.x];
    RWStructuredBuffer<uint> column_offsets = ResourceDescriptorHeap[descriptor_indices.column_offsets.x];
    uint2 extent;
    source.GetDimensions(extent.x, extent.y);
    uint column = p_dispatch_thread_id.x;
    if (column == 0) {
        column_offsets[0] = extent.x;
        column_offsets[WBE_PIXEL_SORT_COLUMN_OFFSETS_START + extent.x] = 0;
    }
    if (column >= extent.x) {
        return;
    }
    uint count = 0;
    for (uint row = 0; row < extent.y; ++row) {
        if (pixel_sort_in_contrast(source.Load(int3(column, row, 0)).rgb, contrast)) {
            ++count;
        }
    }
    column_offsets[WBE_PIXEL_SORT_COLUMN_OFFSETS_START + column] = count;
}
