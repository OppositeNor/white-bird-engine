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
#include "draw_cube/draw_cube_utils.slangh"
#include "defs.slangh"

[[vk::binding(0, 0)]] ConstantBuffer<CameraInfo> camera_info;

VSOutput main(Vertex3DTexturedInput p_input) {
    VSOutput output;
    float4 pos = float4(p_input.position.xyz, 1.0);
    output.position = mul(camera_info.proj, mul(camera_info.view, pos));
    output.normal = p_input.normal;
    output.uv = p_input.uv;

    return output;
}


