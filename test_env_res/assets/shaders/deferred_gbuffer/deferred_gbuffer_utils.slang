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

/**
 * @brief Vertex shader output / fragment shader input for the deferred G-buffer pass.
 */
struct VSOutput {
    /**
 * @brief Clip-space position.
     */
    float4 position       : SV_Position;
    /**
 * @brief World-space position of the fragment.
     */
    float3 world_position : TEXCOORD0;
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

#endif
