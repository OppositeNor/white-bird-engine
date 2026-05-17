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
#include "draw_cube_unshaded/draw_cube_unshaded_utils.slang"
#include "defs.slang"

/**
 * @brief Visualize the interpolated surface normal as an RGB color, mapped from [-1, 1] to [0, 1].
 * @param p_input The interpolated vertex output.
 * @return The visualized normal as an opaque sRGB-ish color.
 */
float4 main(VSOutput p_input) : SV_Target0 {
    return float4(p_input.normal * 0.5 + 0.5, 1.0);
}
