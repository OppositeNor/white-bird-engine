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
#include "defs.slang"

/**
 * @brief Vertex shader output / fragment shader input for the spot-light shadow map pass.
 */
struct ShadowVSOutput {
    /// Clip-space position of the vertex in light space.
    float4 position : SV_Position;
};

/**
 * @brief Constant buffer providing the spot light's view and projection matrices.
 */
ConstantBuffer<CameraInfo> light_camera_info;

/**
 * @brief Transform a vertex into the spot light's clip space for depth-only rendering.
 * @param p_input The incoming vertex attributes.
 * @return The clip-space vertex output for the shadow map pass.
 */
[shader("vertex")]
ShadowVSOutput vertex_main(Vertex3DTexturedInput p_input) {
    ShadowVSOutput output;
    float4 world_position = float4(p_input.position.xyz, 1.0);
    output.position = mul(light_camera_info.proj, mul(light_camera_info.view, world_position));
    return output;
}

/**
 * @brief Depth-only fragment stage. Writes a constant value into the dummy color attachment; the depth attachment is the real output.
 * @param p_input Interpolated vertex output.
 * @return Dummy color value (never read).
 */
[shader("fragment")]
float4 fragment_main(ShadowVSOutput p_input) : SV_Target0 {
    return float4(0.0, 0.0, 0.0, 1.0);
}