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
#include "draw_triangle/draw_triangle_utils.slang"
#include "defs.slang"

[[vk::binding(0, 0)]] ConstantBuffer<Time> time;

float4 main(VSOutput p_input) : SV_Target0 {
    return float4(0.0, (sin(time.duration_sec) + 1) * 0.5, 0.0, 1.0);
}

