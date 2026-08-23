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
#ifndef WBE_FILE_DEFS_SLANG
#define WBE_FILE_DEFS_SLANG

/**
 * @brief The input data from the vertex buffer
 */
struct Vertex3DTexturedInput {
    /**
 * @brief The position of the vertex.
     */
    [[vk::location(0)]] float3 position;
    /**
 * @brief The normal of the vertex.
     */
    [[vk::location(1)]] float3 normal;
    /**
 * @brief The tanglent vector of the vertex.
     */
    [[vk::location(2)]] float3 tangent;
    /**
 * @brief The bitangent vector of the vertex.
     */
    [[vk::location(3)]] float3 bitangent;
    /**
 * @brief The uv value.
     */
    [[vk::location(4)]] float2 uv;
};

/**
 * @brief The information of the camera.
 */
struct CameraInfo {
    /**
 * @brief The view matrix.
     */
    float4x4 view;
    /**
 * @brief The projection matrix.
     */
    float4x4 proj;
    /**
 * @brief The world-space camera position.
     */
    float4 world_position;
};

/**
 * @brief Time information.
 */
struct Time {
    /**
 * @brief The duration from the start of the application until now.
     */
    float duration_sec;
};

/** Maximum number of bindless material entries addressable by one draw pass. */
static const uint WBE_MAX_MATERIAL_COUNT = 32;

/** Fixed bindless texture table contract. Applications may only vary the parameter's base variable name. */
struct MaterialTextures {
    /** Sampler shared by all material textures. */
    SamplerState material_sampler;
    /** Bindless texture array indexed through MaterialParams. */
    Texture2D<float4> textures[];
};

/** Fixed descriptor indices for one material's textures. */
struct MaterialParams {
    /** Albedo texture descriptor index. */
    int albedo_index;
    /** Normal texture descriptor index. */
    int normal_index;
    /** Roughness-metallic-ambient-occlusion texture descriptor index. */
    int roughness_metallic_ao_index;
};

/** Fixed descriptor-index table for every material addressed by an indirect draw. */
struct MaterialParamsBuffer {
    /** Material entries indexed by the indirect draw's material index. */
    MaterialParams materials[WBE_MAX_MATERIAL_COUNT];
};

#endif
