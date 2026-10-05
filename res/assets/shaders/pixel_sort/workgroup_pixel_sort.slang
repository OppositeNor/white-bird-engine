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
 * @file workgroup_pixel_sort.slang
 * @brief Compute pass that sorts the pixels of every row of each 64x64 image chunk by luminance.
 * @details Each 64-invocation workgroup owns one 64x64 chunk, so the dispatch covers the destination in 64x64 workgroups.
 * The chunk rows are sorted 4 at a time with workgroup_radix_sort_64_256. Every key packs the row within the batch into
 * the 2 most significant bits, the quantized luminance into the next 24 bits, and the source column into the 6 least
 * significant bits, so each row stays in its own 64-element range of the sorted array, darker pixels move left, and the
 * column of every sorted key locates its source pixel. Columns past the right edge of the image get the largest
 * luminance, so they sort after every column inside the image.
 */
#include "sort/radix_sort_utils.slang"
#include "utils.slang"

#define WBE_PIXEL_SORT_CHUNK_SIZE 64
#define WBE_PIXEL_SORT_ROWS_PER_BATCH 4
#define WBE_PIXEL_SORT_COLUMN_BIT_SIZE 6
#define WBE_PIXEL_SORT_LUMINANCE_BIT_SIZE 24
#define WBE_PIXEL_SORT_LUMINANCE_MASK ((1u << WBE_PIXEL_SORT_LUMINANCE_BIT_SIZE) - 1)
#define WBE_PIXEL_SORT_ROW_BIT_START (WBE_PIXEL_SORT_COLUMN_BIT_SIZE + WBE_PIXEL_SORT_LUMINANCE_BIT_SIZE)

/** Descriptor indices of the resources used by the pixel sort pass. */
struct WorkgroupPixelSortDescriptorIndices {
    /** Image whose pixels are sorted. */
    [WBE_DESCRIPTOR("sampled_image", "pixel_sort_source")] uint2 source;
    /** Image receiving the sorted pixels; must have the extent of source. */
    [WBE_DESCRIPTOR("storage_image", "pixel_sort_destination", "write")] uint2 destination;
};

[[vk::push_constant]]
ConstantBuffer<WorkgroupPixelSortDescriptorIndices> descriptor_indices;

groupshared uint sort_keys[256];
// The extra trailing element is the zero element the prefix sum clamps out-of-range reads to.
groupshared uint sort_scratch[256 + 1];

/**
 * @brief Build the sort key of one pixel in the current row batch.
 * @param p_source The image whose pixels are sorted.
 * @param p_source_extent The extent of p_source.
 * @param p_chunk_origin The image coordinate of the chunk's top-left pixel.
 * @param p_batch_row The row index within the batch, in [0, WBE_PIXEL_SORT_ROWS_PER_BATCH).
 * @param p_chunk_row The row index within the chunk.
 * @param p_column The column index within the chunk.
 * @return The packed key.
 */
uint make_pixel_sort_key(Texture2D<float4> p_source, uint2 p_source_extent, uint2 p_chunk_origin, uint p_batch_row,
        uint p_chunk_row, uint p_column) {
    uint2 pixel = p_chunk_origin + uint2(p_column, p_chunk_row);
    uint quantized_luminance = WBE_PIXEL_SORT_LUMINANCE_MASK;
    if (all(pixel < p_source_extent)) {
        float3 color = p_source.Load(int3(pixel, 0)).rgb;
        float luminance = saturate(dot(color, float3(0.2126, 0.7152, 0.0722)));
        quantized_luminance = uint(luminance * float(WBE_PIXEL_SORT_LUMINANCE_MASK));
    }
    return (p_batch_row << WBE_PIXEL_SORT_ROW_BIT_START) | (quantized_luminance << WBE_PIXEL_SORT_COLUMN_BIT_SIZE)
        | p_column;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    Texture2D<float4> source = ResourceDescriptorHeap[descriptor_indices.source.x];
    RWTexture2D<float4> destination = ResourceDescriptorHeap[descriptor_indices.destination.x];
    uint2 source_extent;
    source.GetDimensions(source_extent.x, source_extent.y);
    uint2 chunk_origin = p_group_id.xy * WBE_PIXEL_SORT_CHUNK_SIZE;
    uint thread_id = p_group_thread_id.x;
    // Each invocation only touches its own 4 entries of sort_keys outside the sort, and the sort synchronizes before it
    // returns, so the batches need no barrier between reading one result and filling the next keys.
    for (uint batch_start = 0; batch_start < WBE_PIXEL_SORT_CHUNK_SIZE; batch_start += WBE_PIXEL_SORT_ROWS_PER_BATCH) {
        [unroll] for (uint i = 0; i < 4; ++i) {
            uint key_index = thread_id * 4 + i;
            uint batch_row = key_index / WBE_PIXEL_SORT_CHUNK_SIZE;
            uint column = key_index % WBE_PIXEL_SORT_CHUNK_SIZE;
            sort_keys[key_index] =
                make_pixel_sort_key(source, source_extent, chunk_origin, batch_row, batch_start + batch_row, column);
        }
        GroupMemoryBarrierWithGroupSync();
        workgroup_radix_sort_64_256(sort_keys, thread_id, sort_scratch);
        [unroll] for (uint i = 0; i < 4; ++i) {
            uint key_index = thread_id * 4 + i;
            uint chunk_row = batch_start + key_index / WBE_PIXEL_SORT_CHUNK_SIZE;
            uint2 destination_pixel = chunk_origin + uint2(key_index % WBE_PIXEL_SORT_CHUNK_SIZE, chunk_row);
            if (any(destination_pixel >= source_extent)) {
                continue;
            }
            uint source_column = sort_keys[key_index] & (WBE_PIXEL_SORT_CHUNK_SIZE - 1);
            uint2 source_pixel = chunk_origin + uint2(source_column, chunk_row);
            destination[destination_pixel] = float4(source.Load(int3(source_pixel, 0)).rgb, 1.0);
        }
    }
}
