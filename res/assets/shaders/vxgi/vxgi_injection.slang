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
#include "octree/octree_utils.slang"
#include "octree/octree_dynamic_read.slang"

struct DescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer", "octree_info")]
    uint2 octree_info;
    [WBE_DESCRIPTOR("storage_buffer", "octree_node_buffer")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "octree_leaf_buffer", "read")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("storage_buffer", "vxgi_data_buffer", "read")]
    uint2 vxgi_data;
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
    StructuredBuffer<OctreeInfo> octree_info = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
}
