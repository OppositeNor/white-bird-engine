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
 * @brief Vertex shader output / fragment shader input for the main pipeline fullscreen pass.
 */
struct VSOutput {
    /**
 * @brief Clip-space position.
     */
    float4 position : SV_Position;
    /**
 * @brief Screen-space uv used by the fragment stage.
     */
    float2 uv       : TEXCOORD0;
};

/**
 * @brief Emit one vertex of a fullscreen triangle covering the viewport.
 * @param p_vertex_id The SV_VertexID for the current vertex (0..2).
 * @return The vertex output containing clip-space position and screen-space uv coordinates.
 */
[shader("vertex")]
VSOutput vertex_main(uint p_vertex_id : SV_VertexID) {
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

/**
 * @brief Visualize the interpolated uv coordinate as an RG color.
 * @param p_input The interpolated vertex output.
 * @return The uv coordinate packed into the rg channels with b = 0, a = 1.
 */
[shader("fragment")]
float4 fragment_main(VSOutput p_input) : SV_Target0 {
    return float4(p_input.uv, 0.0, 1.0);
}
