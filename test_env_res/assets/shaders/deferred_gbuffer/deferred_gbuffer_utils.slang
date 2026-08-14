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
#ifndef WBE_FILE_DEFERRED_GBUFFER_UTILS_SLANG
#define WBE_FILE_DEFERRED_GBUFFER_UTILS_SLANG

#include "utils.slang"

static const uint WBE_MAX_MATERIAL_COUNT = 32;

/**
 * @brief Vertex shader output / fragment shader input for the deferred G-buffer pass.
 */
struct VSOutput {
    /**
 * @brief Clip-space position.
     */
    float4 position       : SV_Position;
    /**
     * @brief World-space position in xyz. The w component stores the indirect draw's material-table index.
     */
    float4 world_position : TEXCOORD0;
    /**
 * @brief Object-space normal interpolated from the vertex attribute.
     */
    float3 normal         : TEXCOORD1;
    /**
 * @brief Object-space tangent interpolated from the vertex attribute.
     */
    float3 tangent        : TEXCOORD2;
    /**
 * @brief Object-space bitangent interpolated from the vertex attribute.
     */
    float3 bitangent      : TEXCOORD3;
    /**
 * @brief Texture coordinate interpolated from the vertex attribute.
     */
    float2 uv             : TEXCOORD4;
};

/**
 * @brief Bindless material textures sampled by the G-buffer pass.
 */
struct MaterialTextures {
    SamplerState material_sampler;
    Texture2D<float4> textures[];
};

/**
 * @brief Descriptor indices for the active material textures.
 */
struct MaterialParams {
    int albedo_index;
    int normal_index;
    int roughness_metallic_ao_index;
};

/**
 * @brief Descriptor-index table for every material addressed by an indirect draw.
 */
struct MaterialParamsBuffer {
    MaterialParams materials[WBE_MAX_MATERIAL_COUNT];
};

/**
 * @brief Compute a world-space surface normal from a tangent-space normal map sample.
 * @param p_sampled_normal The normal-map sample with components in [0, 1].
 * @param p_normal The interpolated surface normal.
 * @param p_tangent The interpolated surface tangent.
 * @param p_bitangent The interpolated surface bitangent.
 * @return The unit-length world-space normal.
 */
float3 normal_from_texture(float3 p_sampled_normal, float3 p_normal, float3 p_tangent, float3 p_bitangent) {
    float3 normal = normalize(p_normal);
    float3 tangent = p_tangent;
    float3 bitangent = p_bitangent;
    if (dot(tangent, tangent) < 0.0001 || dot(bitangent, bitangent) < 0.0001) {
        float3 helper_axis = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
        tangent = normalize(cross(helper_axis, normal));
        bitangent = normalize(cross(normal, tangent));
    } else {
        tangent = normalize(tangent - normal * dot(normal, tangent));
        float handedness = dot(cross(normal, tangent), bitangent) < 0.0 ? -1.0 : 1.0;
        bitangent = normalize(cross(normal, tangent)) * handedness;
    }
    float3 tangent_space_normal = normalize(p_sampled_normal * 2.0 - 1.0);
    return normalize(tangent * tangent_space_normal.x + bitangent * tangent_space_normal.y + normal * tangent_space_normal.z);
}

#endif
