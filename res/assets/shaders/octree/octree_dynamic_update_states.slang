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
#include "utils/utils.slang"
#include "octree/octree_utils.slang"
#include "octree/octree_dynamic_types.slang"
/**
 * @brief Descriptor-heap resources used by incremental maintenance.
 */
struct DynamicDescriptorIndices {
    /**
     * @brief Descriptor-heap reference to octree_info.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_info")]
    uint2 info;
    /**
     * @brief Descriptor-heap reference to octree_node_buffer.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_node_buffer")]
    uint2 nodes;
    /**
     * @brief Descriptor-heap reference to octree_dynamic_nodes.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_dynamic_nodes")]
    uint2 dynamic_nodes;
    /**
     * @brief Descriptor-heap reference to octree_dynamic_leaves.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_dynamic_leaves")]
    uint2 dynamic_leaves;
    /**
     * @brief Descriptor-heap reference to octree_voxel_claims.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_voxel_claims")]
    uint2 claims;
};
[[vk::push_constant]]
ConstantBuffer<DynamicDescriptorIndices> descriptor_indices;

/**
 * @brief Publishes initialized dynamic child blocks at a pass boundary.
 */
[shader("compute")]
[numthreads(128, 1, 1)]
void main(uint3 p_id : SV_DispatchThreadID) {
    StructuredBuffer<OctreeInfo> info = ResourceDescriptorHeap[descriptor_indices.info.x];
    RWStructuredBuffer<OctreeNode> nodes = ResourceDescriptorHeap[descriptor_indices.nodes.x];
    RWStructuredBuffer<OctreeNode> dynamic_nodes = ResourceDescriptorHeap[descriptor_indices.dynamic_nodes.x];
    uint index = p_id.x;
    bool is_static = index < info[0].max_node_count;
    uint local = is_static ? index : index - info[0].max_node_count;
    if (is_static ? local >= info[0].node_count : local >= info[0].dynamic_node_count) {
        return;
    }
    uint description = is_static ? nodes[local].children_data : dynamic_nodes[local].children_data;
    if ((description >> 29) != uint(OctreeNode::ChildrenState::ALLOCATING_DYNAMIC)) {
        return;
    }
    uint state = (description & 0x1fe00000u) != 0 ? uint(OctreeNode::ChildrenState::LEAF_DYNAMIC)
                                                  : uint(OctreeNode::ChildrenState::NODE_DYNAMIC);
    description = (description & 0x1fffffffu) | (state << 29);
    if (is_static) {
        nodes[local].children_data = description;
    } else {
        dynamic_nodes[local].children_data = description;
    }
}
