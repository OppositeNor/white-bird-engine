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

#include "svogi/svogi_utils.slang"
#include "utils/utils.slang"
#include "light/spot_light_utils.slang"

struct DescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer", "svo_pre_deferred_debug_info_buffer")]
    uint2 svo_info;
    [WBE_DESCRIPTOR("storage_buffer", "svo_node_buffer", "read")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "svo_leaf_buffer", "read")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("storage_image", "atlas", "write")]
    uint2 atlas;
    [WBE_DESCRIPTOR("storage_buffer", "spot_light_infos", "read")]
    uint2 light_infos;
    [WBE_PUSH_CONSTANT("frame_slot")]
    uint shader_debug_frame_slot;
    LightParamDescriptorIndices lights[WBE_MAX_SPOT_LIGHT_COUNT];
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> descriptor_indices;

groupshared uint2 staged_write_buffer[256 * WBE_MAX_SPOT_LIGHT_COUNT];
groupshared uint buffer_tracker;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    ConstantBuffer<SVOInfo> svo_info = ResourceDescriptorHeap[descriptor_indices.svo_info.x];
    StructuredBuffer<SVONode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    StructuredBuffer<SVOLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    RWTexture3D<uint> atlas = ResourceDescriptorHeap[descriptor_indices.atlas.x];
    uint frame_slot = descriptor_indices.shader_debug_frame_slot;

    SVOWriteBrick<uint> svo_radiance = SVOWriteBrick<uint>(SVO(tree_nodes, tree_leaves, svo_info, true), atlas);
    uint spot_light_count = get_spot_light_count(descriptor_indices.light_infos);
    float2 uv = float2(dispatch_thread_id) / float2(uint2(16, 16) * WorkgroupCount().xy);
    if (all(p_group_thread_id.xy == uint2(0, 0))) {
        buffer_tracker = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    // We first gather all the voxels each invocation in this workgroup is going to write into, perform a sort on them, and
    // sum up the ones that are in the same voxel.
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLight spot_light = SpotLight(descriptor_indices.light_infos, descriptor_indices.lights[spot_light_index], spot_light_index);
        float3 position = spot_light.shadow_texel_world_position(uv);
        uint index;
        InterlockedAdd(buffer_tracker, 1, index);
        SVONodeTraceInfo trace_info;
        uint data_index = svo_radiance.svo.trace_data(position, trace_info);
        if (trace_info.is_node()) {
            // Nodes are ignored. We only trace leaves.
            continue;
        }
        // We treat the flux as small VPLs. We inject those VPLs into the voxels.
        // TODO staged_write_buffer[index] = uint2(data_index, );
    }
}
