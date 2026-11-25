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
 * @file pixel_sort_kernel.slang
 * @brief The pixel sort compute kernels, whose entry points the ACP generates; their building blocks are in
 * pixel_sort_utils.slang.
 * @details The count, register, and write kernels sort columns, from the top, by default, and rows, from the left, when
 * WBE_PIXEL_SORT_HORIZONTAL is 1. The column offset buffer holds the line count at index 0 and the value of line l at
 * WBE_PIXEL_SORT_COLUMN_OFFSETS_START + l.
 */
#ifndef WBE_FILE_PIXEL_SORT_KERNEL_SLANG
#define WBE_FILE_PIXEL_SORT_KERNEL_SLANG

#include "utils.slang"
#include "prefix_scan/prefix_scan_utils.slang"
#include "sort/radix_sort_utils.slang"
#include "pixel_sort/pixel_sort_utils.slang"

/**
 * @brief Copy an image into the pixel sort source, so that the sorted pixels can be written back to the copied image in
 * place.
 * @details Dispatch one 16x16 workgroup per 16x16 pixel tile of the input image.
 * @param p_dispatch_thread_id The pixel of the invocation.
 * @param p_input Image whose pixels are sorted.
 * @param p_source Image receiving the copy; must have the extent of p_input.
 */
[wbe::kernel(16, 16, 1)]
void copy_pixel_sort_source(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("pixel_sort_input")] Texture2D<float4> p_input,
    [wbe::bind("pixel_sort_source", "write")] RWTexture2D<float4> p_source) {
    uint2 extent;
    p_input.GetDimensions(extent.x, extent.y);
    uint2 pixel = p_dispatch_thread_id.xy;
    if (any(pixel >= extent)) {
        return;
    }
    p_source[pixel] = p_input.Load(int3(pixel, 0));
}

/**
 * @brief Count the pixels inside the contrast mask in every sorted line of the source image.
 * @details One invocation per line; dispatch ceil(width / 64) workgroups, or ceil(height / 64) for the horizontal
 * variant. The column offset buffer receives the line count at index 0, the mask count of line l at
 * WBE_PIXEL_SORT_COLUMN_OFFSETS_START + l.
 * @param p_dispatch_thread_id The line of the invocation in x.
 * @param p_source Image whose pixels are sorted.
 * @param p_contrast The contrast range of the sorted pixels.
 * @param p_column_offsets Buffer receiving the mask count of every line.
 */
[wbe::kernel(64, 1, 1)]
void count_pixel_sort_mask(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("pixel_sort_source")] Texture2D<float4> p_source,
    [wbe::bind("pixel_sort_contrast")] ConstantBuffer<PixelSortContrast> p_contrast,
    [wbe::bind("pixel_sort_column_offsets", "write")] RWStructuredBuffer<uint> p_column_offsets) {
    uint2 extent;
    p_source.GetDimensions(extent.x, extent.y);
    uint2 line_extent = pixel_sort_line_coordinates(extent);
    uint line = p_dispatch_thread_id.x;
    if (line == 0) {
        p_column_offsets[0] = line_extent.x;
    }
    if (line >= line_extent.x) {
        return;
    }
    uint count = 0;
    for (uint position = 0; position < line_extent.y; ++position) {
        uint2 pixel = pixel_sort_line_pixel(uint2(line, position));
        if (pixel_sort_in_contrast(p_source.Load(int3(pixel, 0)).rgb, p_contrast)) {
            ++count;
        }
    }
    p_column_offsets[WBE_PIXEL_SORT_COLUMN_OFFSETS_START + line] = count;
}

groupshared uint prefix_sum_pixel_sort_columns_scratch[64];

/**
 * @brief Turn the per-line mask counts into the end offset of every sorted line in the key buffer.
 * @details Dispatch exactly one workgroup. After the pass, the keys of line l occupy [offset(l - 1), offset(l)) of the
 * key data, with offset(-1) = 0. The pass does not depend on the sort direction, so both variants share it.
 * @param p_group_thread_id The invocation's index in the workgroup in x.
 * @param p_column_offsets Buffer holding the mask count of every line, summed in place.
 */
[wbe::kernel(64, 1, 1)]
void prefix_sum_pixel_sort_columns(uint3 p_group_thread_id : SV_GroupThreadID,
    [wbe::bind("pixel_sort_column_offsets")] RWStructuredBuffer<uint> p_column_offsets) {
    uint column_count = p_column_offsets[0];
    workgroup_general_prefix_scan<uint, Add<uint>, 64>(p_column_offsets, WBE_PIXEL_SORT_COLUMN_OFFSETS_START, column_count,
        p_group_thread_id.x, prefix_sum_pixel_sort_columns_scratch);
}

/**
 * @brief Register the pixels inside the contrast mask into the radix sort key buffer.
 * @details One invocation per sorted line; dispatch ceil(width / 64) workgroups, or ceil(height / 64) for the horizontal
 * variant. Each invocation walks its line from the start and writes the key of every masked pixel to the next entry of
 * the line's range of the key buffer, and the pixel's rank among the masked pixels of its line to the mask image. Pixels
 * outside the mask get WBE_PIXEL_SORT_UNMASKED. The key buffer receives the total number of masked pixels as its element
 * count, and the number of radix sort local sorts of that count. With the span scope, a span starts at a masked pixel
 * that is the first of its line or follows an unmasked pixel; the invocation measures the span before registering its
 * pixels with span keys, and also writes each pixel's offset in its span and the span length to the mask image.
 * @param p_dispatch_thread_id The line of the invocation in x.
 * @param p_source Image whose pixels are sorted.
 * @param p_contrast The contrast range of the sorted pixels and the metric they are sorted by.
 * @param p_column_offsets The end offset of every line in the key buffer, from prefix_sum_pixel_sort_columns.
 * @param p_keys Radix sort key buffer receiving the keys, their count, and the local sort count.
 * @param p_mask Mask image receiving the line rank of every masked pixel, and with the span scope its span offset and
 * span length; must have the extent of p_source, and be R16G16B16A16_UINT with the span scope or R16_UINT with the line
 * scope.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(64, 1, 1)]
void register_pixel_sort_keys(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("pixel_sort_source")] Texture2D<float4> p_source,
    [wbe::bind("pixel_sort_contrast")] ConstantBuffer<PixelSortContrast> p_contrast,
    [wbe::bind("pixel_sort_column_offsets", "read")] RWStructuredBuffer<uint> p_column_offsets,
    [wbe::bind("pixel_sort_keys", "write")] RWByteAddressBuffer p_keys,
    [wbe::bind("pixel_sort_mask", "write")] RWTexture2D<uint4> p_mask,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    GPUVector<uint> keys = GPUVector<uint>(p_keys, p_frame_slot);
    uint2 extent;
    p_source.GetDimensions(extent.x, extent.y);
    uint2 line_extent = pixel_sort_line_coordinates(extent);
    uint line = p_dispatch_thread_id.x;
    if (line >= line_extent.x) {
        return;
    }
    if (line == line_extent.x - 1) {
        uint element_count = p_column_offsets[WBE_PIXEL_SORT_COLUMN_OFFSETS_START + line];
        keys.set_size(element_count);
        keys.set_header(WBE_RADIX_SORT_LOCAL_SORT_NUM_INDEX, radix_sort_local_sort_num(element_count));
    }
    uint rank = 0;
    if (p_contrast.sort_scope == WBE_PIXEL_SORT_SCOPE_SPAN) {
        uint position = 0;
        while (position < line_extent.y) {
            uint2 pixel = pixel_sort_line_pixel(uint2(line, position));
            if (!pixel_sort_in_contrast(p_source.Load(int3(pixel, 0)).rgb, p_contrast)) {
                p_mask[pixel] = uint4(WBE_PIXEL_SORT_UNMASKED, 0, 0, 0);
                ++position;
                continue;
            }
            // The pixel starts a span, which ends before the next unmasked pixel or at the end of the line.
            uint span_length = 1;
            while (position + span_length < line_extent.y) {
                uint2 next_pixel = pixel_sort_line_pixel(uint2(line, position + span_length));
                if (!pixel_sort_in_contrast(p_source.Load(int3(next_pixel, 0)).rgb, p_contrast)) {
                    break;
                }
                ++span_length;
            }
            uint span_start = pixel_sort_key_index(p_column_offsets, line, rank);
            for (uint offset = 0; offset < span_length; ++offset) {
                uint2 span_pixel = pixel_sort_line_pixel(uint2(line, position + offset));
                float3 color = p_source.Load(int3(span_pixel, 0)).rgb;
                keys.set(span_start + offset,
                    pixel_sort_make_span_key(color, p_contrast.sort_metric, span_start, span_length, offset, extent));
                p_mask[span_pixel] = uint4(rank + offset, offset, span_length, 0);
            }
            rank += span_length;
            position += span_length;
        }
        return;
    }
    for (uint position = 0; position < line_extent.y; ++position) {
        uint2 pixel = pixel_sort_line_pixel(uint2(line, position));
        float3 color = p_source.Load(int3(pixel, 0)).rgb;
        if (pixel_sort_in_contrast(color, p_contrast)) {
            keys.set(pixel_sort_key_index(p_column_offsets, line, rank),
                pixel_sort_make_key(color, p_contrast.sort_metric, pixel, extent));
            p_mask[pixel] = uint4(rank, 0, 0, 0);
            ++rank;
        } else {
            p_mask[pixel] = uint4(WBE_PIXEL_SORT_UNMASKED, 0, 0, 0);
        }
    }
}

/**
 * @brief Write the pixels of the source image in the order of the sorted radix sort keys.
 * @details Dispatch one 16x16 workgroup per 16x16 pixel tile. A pixel inside the contrast mask receives the source pixel
 * of the sorted key at its line rank, which register_pixel_sort_keys stored in the mask image. That key belongs to the
 * same line, so the masked pixels of every line end up sorted by the metric selected in PixelSortContrast, from the
 * smallest value, top to bottom in the vertical variant and left to right in the horizontal variant. With the span
 * scope, the key belongs to the same span, whose pixels are consecutive, so the source pixel is the pixel of the span at
 * the offset the key encodes, and every span ends up sorted on its own. Other pixels keep their source color.
 * @param p_dispatch_thread_id The pixel of the invocation.
 * @param p_source Image whose pixels are sorted.
 * @param p_keys Radix sort key buffer holding the sorted keys.
 * @param p_mask Mask image holding the line rank, span offset, and span length of every masked pixel, from
 * register_pixel_sort_keys.
 * @param p_contrast The scope of the sort.
 * @param p_column_offsets The end offset of every line in the key buffer, from prefix_sum_pixel_sort_columns.
 * @param p_destination Image receiving the sorted pixels; must have the extent of p_source.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(16, 16, 1)]
void write_sorted_pixels(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("pixel_sort_source")] Texture2D<float4> p_source,
    [wbe::bind("pixel_sort_keys", "read")] RWByteAddressBuffer p_keys,
    [wbe::bind("pixel_sort_mask", "read")] RWTexture2D<uint4> p_mask,
    [wbe::bind("pixel_sort_contrast")] ConstantBuffer<PixelSortContrast> p_contrast,
    [wbe::bind("pixel_sort_column_offsets", "read")] RWStructuredBuffer<uint> p_column_offsets,
    [wbe::bind("pixel_sort_destination", "write")] RWTexture2D<float4> p_destination,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    GPUVector<uint> keys = GPUVector<uint>(p_keys, p_frame_slot);
    uint2 extent;
    p_source.GetDimensions(extent.x, extent.y);
    uint2 pixel = p_dispatch_thread_id.xy;
    if (any(pixel >= extent)) {
        return;
    }
    uint4 mask_value = p_mask[pixel];
    uint rank = mask_value.x;
    uint2 source_pixel = pixel;
    if (rank != WBE_PIXEL_SORT_UNMASKED) {
        uint2 line_coordinates = pixel_sort_line_coordinates(pixel);
        uint key_index = pixel_sort_key_index(p_column_offsets, line_coordinates.x, rank);
        uint key = keys.get(key_index);
        if (p_contrast.sort_scope == WBE_PIXEL_SORT_SCOPE_SPAN) {
            uint offset = mask_value.y;
            uint span_length = mask_value.z;
            uint source_offset = pixel_sort_span_key_offset(key, key_index - offset, span_length, extent);
            source_pixel = pixel_sort_line_pixel(uint2(line_coordinates.x, line_coordinates.y - offset + source_offset));
        } else {
            source_pixel = pixel_sort_key_pixel(key, line_coordinates.x, extent);
        }
    }
    p_destination[pixel] = float4(p_source.Load(int3(source_pixel, 0)).rgb, 1.0);
}

/** Parameters of the pixel sorting post-processing effects. */
struct PixelSortingPostProcessingParams {
    /** The exponent every color channel is raised to; 1 leaves the color unchanged. */
    float gamma;
};

/**
 * @brief Compute the post-processed color of one pixel.
 * @param p_sorted_image The pixel-sorted image.
 * @param p_sorted_sampler Clamp-to-edge sampler for filtered reads of p_sorted_image.
 * @param p_pixel The pixel to compute; inside p_extent.
 * @param p_extent The extent of p_sorted_image.
 * @param p_params The parameters of the post-processing effects.
 * @return The color of p_pixel in the output image.
 */
float4 pixel_sorting_post_process(Texture2D<float4> p_sorted_image,
    SamplerState p_sorted_sampler,
    uint2 p_pixel,
    uint2 p_extent,
    PixelSortingPostProcessingParams p_params) {
    float3 color = p_sorted_image.Load(int3(p_pixel, 0)).rgb;
    color = pow(max(color, 0.0), p_params.gamma);
    return float4(color, 1.0);
}

/**
 * @brief Apply general post-processing effects to the pixel-sorted image.
 * @details Dispatch one 16x16 workgroup per 16x16 pixel tile of the sorted image. Every invocation writes one output
 * pixel, and may read any pixel of the sorted image, so effects that sample neighbors, such as blurs, are allowed. The
 * effects belong in pixel_sorting_post_process(); it currently applies gamma correction.
 * @param p_dispatch_thread_id The pixel of the invocation.
 * @param p_sorted_image The pixel-sorted image, from write_sorted_pixels.
 * @param p_sorted_sampler Clamp-to-edge sampler for filtered reads of the sorted image.
 * @param p_params The parameters of the post-processing effects.
 * @param p_output Image receiving the post-processed pixels; must have the extent of p_sorted_image.
 */
[wbe::kernel(16, 16, 1)]
void pixel_sorting_post_processing(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("pixel_sort_destination")] Texture2D<float4> p_sorted_image,
    [wbe::bind("pixel_sort_sampler")] SamplerState p_sorted_sampler,
    [wbe::bind("pixel_sorting_post_processing_params")] ConstantBuffer<PixelSortingPostProcessingParams> p_params,
    [wbe::bind("pixel_sorting_post_processing_output", "write")] RWTexture2D<float4> p_output) {
    uint2 extent;
    p_sorted_image.GetDimensions(extent.x, extent.y);
    uint2 pixel = p_dispatch_thread_id.xy;
    if (any(pixel >= extent)) {
        return;
    }
    p_output[pixel] = pixel_sorting_post_process(p_sorted_image, p_sorted_sampler, pixel, extent, p_params);
}

#endif
