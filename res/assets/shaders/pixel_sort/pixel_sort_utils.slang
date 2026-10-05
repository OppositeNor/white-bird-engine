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
 * @file pixel_sort_utils.slang
 * @brief Key encoding shared by the pixel sort passes.
 * @details A sorted pixel of a W x H image gets one 32-bit key, (column * L + luminance) * H + row, where L =
 * floor((2^32 - 1) / (W * H)) is the number of luminance levels. Sorting the keys in ascending order groups them by
 * column, orders each column by luminance, and keeps the source row of every key. The keys of column c are registered
 * at the range of the key buffer that starts after the keys of every earlier column, so the sort keeps every column in
 * its range, and the k-th masked pixel of column c, from the top, receives the k-th key of that range.
 */
#ifndef WBE_FILE_PIXEL_SORT_UTILS_SLANG
#define WBE_FILE_PIXEL_SORT_UTILS_SLANG

/** Value used in the pixel sort index image for pixels outside the contrast mask. */
#define WBE_PIXEL_SORT_UNMASKED 0xFFFFFFFFu
/** The first entries of the column offset buffer are metadata; the first one is the column count. */
#define WBE_PIXEL_SORT_COLUMN_OFFSETS_START 4

/**
 * @struct PixelSortContrast
 * @brief The luminance range of the pixels that are sorted, set by the host.
 */
struct PixelSortContrast {
    /** The smallest luminance of a sorted pixel, in [0, 1]. */
    float min_luminance;
    /** The largest luminance of a sorted pixel, in [0, 1]. */
    float max_luminance;
};

/**
 * @brief Get the luminance of a linear color.
 * @param p_color The linear color.
 * @return The luminance, clamped to [0, 1].
 */
float pixel_sort_luminance(float3 p_color) {
    return saturate(dot(p_color, float3(0.2126, 0.7152, 0.0722)));
}

/**
 * @brief Check whether a pixel is inside the contrast mask.
 * @param p_color The linear color of the pixel.
 * @param p_contrast The contrast range.
 * @return Whether the luminance of p_color is within [min_luminance, max_luminance].
 */
bool pixel_sort_in_contrast(float3 p_color, PixelSortContrast p_contrast) {
    float luminance = pixel_sort_luminance(p_color);
    return luminance >= p_contrast.min_luminance && luminance <= p_contrast.max_luminance;
}

/**
 * @brief Get the number of luminance levels a key can hold for an image.
 * @param p_extent The image extent; the host must ensure the result is at least 2.
 * @return floor((2^32 - 1) / (p_extent.x * p_extent.y)).
 */
uint pixel_sort_luminance_levels(uint2 p_extent) {
    return 0xFFFFFFFFu / (p_extent.x * p_extent.y);
}

/**
 * @brief Build the sort key of a pixel.
 * @param p_color The color of the pixel.
 * @param p_pixel The pixel coordinate.
 * @param p_extent The image extent.
 * @return The packed key.
 */
uint pixel_sort_make_key(float3 p_color, uint2 p_pixel, uint2 p_extent) {
    uint luminance_levels = pixel_sort_luminance_levels(p_extent);
    float luminance = pixel_sort_luminance(p_color);
    uint quantized_luminance = min(uint(luminance * float(luminance_levels)), luminance_levels - 1);
    return (p_pixel.x * luminance_levels + quantized_luminance) * p_extent.y + p_pixel.y;
}

/**
 * @brief Get the source row stored in a sort key.
 * @param p_key The key.
 * @param p_extent The image extent.
 * @return The row of the pixel the key was built from.
 */
uint pixel_sort_key_row(uint p_key, uint2 p_extent) {
    return p_key % p_extent.y;
}

#endif
