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

struct CombineLightingDescIndices {
    [wbe::desc("sampled_image", "gbuffer_rma")]
    DescriptorHandle<Texture2D<float4>> gbuffer_rma;
    [wbe::desc("sampled_image", "direct_lighting")]
    DescriptorHandle<Texture2D<float4>> direct_lighting;
    [wbe::desc("sampled_image", "indirect_lighting")]
    DescriptorHandle<Texture2D<float4>> indirect_lighting;
    [wbe::desc("storage_image", "result_image")]
    DescriptorHandle<RWTexture2D<float4>> result_image;
};

[vk::push_constant]
ConstantBuffer<CombineLightingDescIndices> di;

/**
 * @brief Combine direct lighting with indirect lighting attenuated by the G-buffer ambient occlusion.
 * @param p_group_id Dispatched workgroup index.
 * @param p_group_thread_id Thread index within the workgroup.
 */
[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    Texture2D<float4> gbuffer_rma = di.gbuffer_rma;
    Texture2D<float4> direct_lighting = di.direct_lighting;
    Texture2D<float4> indirect_lighting = di.indirect_lighting;
    RWTexture2D<float4> result_image = di.result_image;
    uint width;
    uint height;
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    result_image.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height) {
        return;
    }
    float ambient_occlusion = clamp(gbuffer_rma.Load(int3(dispatch_thread_id, 0)).b, 0.0, 1.0);
    float3 direct = direct_lighting.Load(int3(dispatch_thread_id, 0)).rgb;
    float3 indirect = indirect_lighting.Load(int3(dispatch_thread_id, 0)).rgb;
    result_image[dispatch_thread_id] = float4(direct + indirect * ambient_occlusion, 1.0);
}
