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

#ifndef WBE_FILE_LIGHT_UTILS_SLANG
#define WBE_FILE_LIGHT_UTILS_SLANG

/**
 * @brief Maximum number of spot lights a lighting pass evaluates.
 */
static const uint WBE_MAX_SPOT_LIGHT_COUNT = 4;

/**
 * @brief Spot light parameters shared by the lighting, shadow, and RSM passes. Cone angles are stored as cosines for cheap evaluation.
 */
struct SpotLightInfo {
    /// World-to-light view matrix used to sample the shadow map.
    float4x4 view;
    /// Light-space projection matrix used to sample the shadow map.
    float4x4 proj;
    /// Inverse light-space view-projection used to reconstruct world positions from shadow-map depth.
    float4x4 inv_view_proj;
    /// World-space position of the spot light.
    float3 position;
    /// Scalar intensity multiplier.
    float intensity;
    /// World-space direction the spot is pointing (unit vector).
    float3 direction;
    /// Cosine of the inner cone half-angle (full intensity inside).
    float inner_cone_cos;
    /// Linear-space color of the spot light.
    float3 color;
    /// Cosine of the outer cone half-angle (zero intensity outside).
    float outer_cone_cos;
    /// Maximum effective light distance.
    float range;
    /// Projected emitter width in world units.
    float emitter_width;
};

/**
 * @brief Spot light uniform payload holding every active spot light.
 */
struct SpotLightInfoBuffer {
    /// Active spot lights. Entries beyond spot_light_count are ignored.
    SpotLightInfo spot_light_info[WBE_MAX_SPOT_LIGHT_COUNT];
    /// Number of active spot light entries in spot_light_info.
    uint spot_light_count;
};

#endif
