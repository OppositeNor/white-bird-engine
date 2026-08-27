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

Texture2D<float4> gbuffer_rma;
Texture2D<float> gbuffer_depth;
Texture2D<float4> direct_lighting_texture;
Texture2D<float4> indirect_lighting_texture;
SamplerState gbuffer_sampler;
RWTexture2D<float4> post_processed_output;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    uint width;
    uint height;
    uint2 dispatch_thread_id = z_order_swizzle_8x8(p_group_id.xy, p_group_thread_id.xy);
    post_processed_output.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
    float2 uv = (float2(dispatch_thread_id) + 0.5) / float2(width, height);
    float depth = gbuffer_depth.SampleLevel(gbuffer_sampler, uv, 0.0);
    if (depth <= 0.0) {
        post_processed_output[dispatch_thread_id] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float4 rma_flags = gbuffer_rma.SampleLevel(gbuffer_sampler, uv, 0.0);
    float ambient_occlusion = clamp(rma_flags.b, 0.0, 1.0);
    float3 direct_lighting = direct_lighting_texture.Load(int3(dispatch_thread_id, 0)).rgb;
    float3 indirect_lighting = indirect_lighting_texture.Load(int3(dispatch_thread_id, 0)).rgb;
    float3 color = (direct_lighting + indirect_lighting) * ambient_occlusion;
    color = color / (color + float3(1.0));
    post_processed_output[dispatch_thread_id] = float4(color, 1.0);
}
