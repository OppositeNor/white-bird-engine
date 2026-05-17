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

#endif
