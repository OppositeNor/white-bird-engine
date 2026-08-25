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

/** Marks a vertex entry-point field and selects a resource attribute by role. */
[__AttributeUsage(_AttributeTargets.Var)]
struct WBE_VERTEX_INPUTAttribute {
    /** Vertex attribute role; an empty value selects the annotated field name. */
    string vertex_role = "";
};

/**
 * @brief The input data from the vertex buffer
 */
struct Vertex3DTexturedInput {
    /**
 * @brief The position of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 position;
    /**
 * @brief The normal of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 normal;
    /**
 * @brief The tanglent vector of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 tangent;
    /**
 * @brief The bitangent vector of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 bitangent;
    /**
 * @brief The uv value.
     */
    [WBE_VERTEX_INPUT]
    float2 uv;
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

/** Marks an integer material parameter as the descriptor index for one texture role. */
[__AttributeUsage(_AttributeTargets.Var)]
struct WBE_TEXTUREAttribute {
    /** Texture role; an empty value selects the annotated variable name. */
    string texture_role = "";
};

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
    [WBE_TEXTURE("albedo")]
    int albedo_index;
    /** Normal texture descriptor index. */
    [WBE_TEXTURE("normal")]
    int normal_index;
    /** RMA texture descriptor index. */
    [WBE_TEXTURE("rma")]
    int rma_index;
};

/** Fixed descriptor-index table for every material addressed by an indirect draw. */
struct MaterialParamsBuffer {
    /** Material entries indexed by the indirect draw's material index. */
    MaterialParams materials[WBE_MAX_MATERIAL_COUNT];
};

#endif
