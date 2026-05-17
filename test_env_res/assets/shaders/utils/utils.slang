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
#ifndef WBE_FILE_UTILS_SLANG
#define WBE_FILE_UTILS_SLANG

/**
 * @brief Mathematical constant pi.
 */
static const float WBE_PI = 3.14159265359;

/**
 * @brief Helper used by ::octahedron_encode to fold the lower hemisphere onto the unit square.
 * @param p_v The xy components of a normalized direction projected onto the octahedron.
 * @return The folded xy coordinates clamped to the [-1, 1] square.
 */
float2 octahedron_wrap(float2 p_v) {
    return (1.0 - abs(p_v.yx)) * (float2(p_v.x >= 0.0 ? 1.0 : -1.0, p_v.y >= 0.0 ? 1.0 : -1.0));
}

/**
 * @brief Encode a unit-length direction vector into a 2-component value in [-1, 1] using octahedron mapping.
 * @param p_n The unit-length direction to encode.
 * @return The encoded 2D coordinate, suitable for storage in an R16G16_SNORM (or higher precision) target.
 */
float2 octahedron_encode(float3 p_n) {
    float3 n = p_n / max(abs(p_n.x) + abs(p_n.y) + abs(p_n.z), 0.00001);
    float2 r = n.z >= 0.0 ? n.xy : octahedron_wrap(n.xy);
    return clamp(r, -1.0, 1.0);
}

/**
 * @brief Decode an octahedron-mapped 2D coordinate back into a unit-length direction vector.
 * @param p_f The encoded 2D coordinate, as produced by ::octahedron_encode.
 * @return The decoded unit-length direction.
 */
float3 octahedron_decode(float2 p_f) {
    float3 n = float3(p_f.x, p_f.y, 1.0 - abs(p_f.x) - abs(p_f.y));
    float t = saturate(-n.z);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

// -----------------------------------------------------------------------------
// Color space helpers.
// -----------------------------------------------------------------------------

/**
 * @brief Convert a linear-space color to sRGB using the approximate gamma 2.2 curve.
 * @param p_color The linear-space color (non-negative components).
 * @return The sRGB-encoded color.
 */
float3 linear_to_srgb(float3 p_color) {
    return pow(max(p_color, float3(0.0)), float3(1.0 / 2.2));
}

/**
 * @brief Convert an sRGB-encoded color to linear space using the approximate gamma 2.2 curve.
 * @param p_color The sRGB-encoded color (non-negative components).
 * @return The linear-space color.
 */
float3 srgb_to_linear(float3 p_color) {
    return pow(max(p_color, float3(0.0)), float3(2.2));
}

#endif
