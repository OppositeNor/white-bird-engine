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
#include "draw_triangle/draw_triangle_utils.slang"
#include "defs.slang"

/**
 * @brief Constant buffer providing the current application time.
 */
ConstantBuffer<Time> time;

/**
 * @brief Transform a textured vertex into a 2D triangle in clip space with the y axis flipped to match Vulkan conventions.
 * @param p_input The incoming vertex attributes.
 * @return The vertex output for the animated triangle debug pass.
 */
[shader("vertex")]
VSOutput vertex_main(Vertex3DTexturedInput p_input) {
    VSOutput output;

    output.position = float4(p_input.position.x, -1.0 * p_input.position.y, 1.0, 1.0);
    output.uv = p_input.uv;

    return output;
}

/**
 * @brief Output a green color whose intensity is animated as a sine of the application time.
 * @param p_input The interpolated vertex output (unused).
 * @return The animated opaque color.
 */
[shader("fragment")]
float4 fragment_main(VSOutput p_input) : SV_Target0 {
    return float4(0.0, (sin(time.duration_sec) + 1) * 0.5, 0.0, 1.0);
}