// vertex
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
#include "deferred_gbuffer/deferred_gbuffer_utils.slang"
#include "defs.slang"

/**
 * @brief Constant buffer providing the per-frame view and projection matrices.
 */
[[vk::binding(0, 0)]] ConstantBuffer<CameraInfo> camera_info;

/**
 * @brief Transform a textured vertex into clip space and pass per-vertex attributes through to the fragment stage.
 * @param p_input The incoming vertex attributes.
 * @return The vertex output for the deferred G-buffer pass.
 */
VSOutput main(Vertex3DTexturedInput p_input) {
    VSOutput output;
    float4 world_position = float4(p_input.position.xyz, 1.0);
    output.position       = mul(camera_info.proj, mul(camera_info.view, world_position));
    output.world_position = p_input.position;
    output.normal         = p_input.normal;
    output.tangent        = p_input.tangent;
    output.bitangent      = p_input.bitangent;
    output.uv             = p_input.uv;
    return output;
}
