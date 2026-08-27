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
#include "deferred_composition/deferred_composition_utils.slang"

[shader("vertex")]
VSOutput vertex_main(uint p_vertex_id : SV_VulkanVertexID) {
    return build_fullscreen_triangle(p_vertex_id);
}

Texture2D<float4> post_processed_texture;
SamplerState post_processed_sampler;

[shader("fragment")]
float4 fragment_main(VSOutput p_input) : SV_Target0 {
    return post_processed_texture.Sample(post_processed_sampler, p_input.uv);
}
