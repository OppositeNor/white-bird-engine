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
#include "svo/svo_utils.slang"
#include "svo/svo.slang"

struct DrawIndexedIndirectCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

struct SVODebugDrawFindNodesDescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer", "svo_info")]
    uint2 svo_info;
    [WBE_DESCRIPTOR("storage_buffer", "svo_node_buffer", "read")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "svo_leaf_buffer", "read")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("uniform_buffer", "svo_debug_draw_settings")]
    uint2 settings;
    [WBE_DESCRIPTOR("storage_buffer", "svo_debug_draw_indirect_buffer")]
    uint2 indirect_draw;
    [WBE_DESCRIPTOR("storage_buffer", "svo_debug_draw_node_list", "write")]
    uint2 node_list;
    [WBE_PUSH_CONSTANT("frame_slot")]
    uint shader_debug_frame_slot;
};

[vk::push_constant]
ConstantBuffer<SVODebugDrawFindNodesDescriptorIndices> descriptor_indices;

bool has_children(SVONode p_node) {
    SVONode::ChildrenState state = p_node.get_children_state();
    return state == SVONode::ChildrenState::NODE || state == SVONode::ChildrenState::NODE_DYNAMIC ||
        state == SVONode::ChildrenState::LEAF || state == SVONode::ChildrenState::LEAF_DYNAMIC;
}

/**
 * @brief Appends every node at the drawn depth to the debug draw's node list, after the instance svo_debug_draw_setup
 * reserves for the bounds.
 * @details One invocation per block of eight sibling leaves starts at their shared parent and walks up one level at a
 * time. At each level only the invocation of the first sibling with children continues to the parent; every node with
 * children has a leaf block below it, so exactly one invocation reaches each node and appends it. Blocks the parent does not
 * reference, such as static blocks superseded by dynamic ones, are dropped. The root's sectors, whose indices are fixed,
 * and the leaves are drawn without this search.
 */
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint p_thread_id : SV_DispatchThreadID) {
    ConstantBuffer<SVOInfo> svo_info = ResourceDescriptorHeap[descriptor_indices.svo_info.x];
    StructuredBuffer<SVONode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    StructuredBuffer<SVOLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    ConstantBuffer<SVODebugDrawSettings> settings = ResourceDescriptorHeap[descriptor_indices.settings.x];
    RWStructuredBuffer<DrawIndexedIndirectCommand> indirect_draw = ResourceDescriptorHeap[descriptor_indices.indirect_draw.x];
    RWStructuredBuffer<uint> node_list = ResourceDescriptorHeap[descriptor_indices.node_list.x];
    uint frame_slot = descriptor_indices.shader_debug_frame_slot;
    SVO svo = SVO(tree_nodes, tree_leaves, svo_info, true);

    uint target_depth = settings.depth + 1;
    if (target_depth <= 1 || target_depth >= svo_info.max_depth) {
        return;
    }
    uint leaf_block = p_thread_id * 8;
    if (leaf_block >= svo.get_total_leaves()) {
        return;
    }
    int parent_index = svo.get_leaf(leaf_block).parent_index;
    if (parent_index < 0) {
        return;
    }
    SVONode parent = svo.get_node(uint(parent_index));
    SVONode::ChildrenState expected_leaf_state =
        leaf_block >= svo_info.leaves_count ? SVONode::ChildrenState::LEAF_DYNAMIC : SVONode::ChildrenState::LEAF;
    if (parent.get_children_state() != expected_leaf_state || parent.get_children_index() != leaf_block) {
        return;
    }
    // Every leaf block has its own parent, at depth max_depth - 1.
    uint current = uint(parent_index);
    for (uint depth = svo_info.max_depth - 1; depth > target_depth; --depth) {
        uint sibling_block = current & ~7u;
        for (uint sibling = sibling_block; sibling < current; ++sibling) {
            if (has_children(svo.get_node(sibling))) {
                return;
            }
        }
        int next_index = svo.get_node(current).parent_index;
        WBE_SHADER_DEBUG_ASSERT(next_index >= 0, ShaderDebugError::SVO_SHALLOWER_THAN_EXPECTED, frame_slot);
        if (next_index < 0) {
            return;
        }
        SVONode next = svo.get_node(uint(next_index));
        SVONode::ChildrenState expected_node_state =
            sibling_block >= svo_info.node_count ? SVONode::ChildrenState::NODE_DYNAMIC : SVONode::ChildrenState::NODE;
        if (next.get_children_state() != expected_node_state || next.get_children_index() != sibling_block) {
            return;
        }
        current = uint(next_index);
    }
    uint instance;
    InterlockedAdd(indirect_draw[0].instance_count, 1, instance);
    // Instance zero draws the bounds.
    node_list[instance - 1] = current;
}
