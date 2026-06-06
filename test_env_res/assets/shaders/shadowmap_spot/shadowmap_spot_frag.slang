// fragment
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
 * @brief Vertex shader output / fragment shader input for the spot-light shadow map pass.
 */
struct ShadowVSOutput {
    /// Clip-space position of the vertex in light space.
    float4 position : SV_Position;
};

/**
 * @brief Depth-only fragment stage. Writes a constant value into the dummy color attachment; the depth attachment is the real output.
 * @param p_input Interpolated vertex output.
 * @return Dummy color value (never read).
 */
float4 main(ShadowVSOutput p_input) : SV_Target0 {
    return float4(0.0, 0.0, 0.0, 1.0);
}
