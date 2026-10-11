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

#include "light/light_utils.slang"
#include "svo/svo_utils.slang"
#include "svo/svo_dynamic_read.slang"

struct DescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "svo_pre_deferred_debug_info_buffer", "read")]
    uint2 svo_info;
    [WBE_DESCRIPTOR("storage_buffer", "svo_node_buffer", "read")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "svo_leaf_buffer", "read")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("storage_buffer", "svo_dynamic_nodes", "read")]
    uint2 dynamic_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "svo_dynamic_leaves", "read")]
    uint2 dynamic_leaves;

    [WBE_DESCRIPTOR("storage_buffer", "svogi_data_buffer", "read")]
    uint2 svogi_data;
    [WBE_DESCRIPTOR("uniform_buffer", "shadow_light_info")]
    uint2 light_camera_info;
    LightParamDescriptorIndices lights[WBE_MAX_SPOT_LIGHT_COUNT];
    [WBE_DESCRIPTOR("uniform_buffer", "spot_light_count")]
    uint2 light_count;
};

[[vk::push_constant]]
ConstantBuffer<DescriptorIndices> descriptor_indices;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    uint2 dispatch_thread_id = z_order_swizzle_8x8(p_group_id.xy, p_group_thread_id.xy);
    StructuredBuffer<SVOInfo> svo_info = ResourceDescriptorHeap[descriptor_indices.svo_info.x];
    StructuredBuffer<SVONode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    StructuredBuffer<SVOLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    StructuredBuffer<SVONode> dynamic_nodes = ResourceDescriptorHeap[descriptor_indices.dynamic_nodes.x];
    StructuredBuffer<SVODynamicLeafBlock> dynamic_leaves = ResourceDescriptorHeap[descriptor_indices.dynamic_leaves.x];

    SVO svo(tree_nodes, dynamic_nodes, tree_leaves, dynamic_leaves, svo_info, true);

    ConstantBuffer<SpotLightInfo> light_camera_info = ResourceDescriptorHeap[descriptor_indices.light_camera_info.x];
    // TODO
}
