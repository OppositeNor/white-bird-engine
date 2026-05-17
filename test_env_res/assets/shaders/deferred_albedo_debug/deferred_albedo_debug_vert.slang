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

/**
 * @brief Vertex shader output / fragment shader input for the deferred albedo debug fullscreen pass.
 */
struct VSOutput {
    /**
 * @brief Clip-space position of the fullscreen triangle vertex.
     */
    float4 position : SV_Position;
    /**
 * @brief Screen-space uv used to sample the G-buffer albedo target.
     */
    float2 uv       : TEXCOORD0;
};

/**
 * @brief Emit one vertex of a fullscreen triangle covering the viewport.
 * @param p_vertex_id The SV_VertexID for the current vertex (0..2).
 * @return The vertex output containing clip-space position and screen-space uv coordinates.
 */
VSOutput main(uint p_vertex_id : SV_VertexID) {
    VSOutput output;

    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };
    float2 uvs[3] = {
        float2(0.0, 0.0),
        float2(0.0, 2.0),
        float2(2.0, 0.0)
    };

    output.position = float4(positions[p_vertex_id], 0.0, 1.0);
    output.uv       = uvs[p_vertex_id];
    return output;
}
