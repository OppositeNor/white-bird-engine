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

struct DrawIndexedIndirectCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

struct SVODebugDrawSetupDescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer", "svo_info")]
    uint2 svo_info;
    [WBE_DESCRIPTOR("storage_buffer", "svo_debug_draw_indirect_buffer")]
    uint2 indirect_draw;
    [WBE_DESCRIPTOR("storage_buffer", "svo_node_buffer", "read")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("uniform_buffer", "svo_debug_draw_settings")]
    uint2 settings;
    [WBE_DESCRIPTOR("storage_buffer", "svo_debug_draw_node_list", "write")]
    uint2 node_list;
};

[vk::push_constant]
ConstantBuffer<SVODebugDrawSetupDescriptorIndices> descriptor_indices;

/**
 * @brief Builds the indexed-indirect command of the post-construction debug view at the drawn depth.
 * @details At the leaves, every leaf instance is followed by the tree's bounds. At a node depth, instance zero draws the
 * bounds and each further instance a node of the node list: the root's sectors with children, whose indices are fixed,
 * are written here, and svo_debug_draw_find_nodes appends the nodes of the other depths.
 */
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    ConstantBuffer<SVOInfo> svo_info = ResourceDescriptorHeap[descriptor_indices.svo_info.x];
    RWStructuredBuffer<DrawIndexedIndirectCommand> indirect_draw = ResourceDescriptorHeap[descriptor_indices.indirect_draw.x];
    StructuredBuffer<SVONode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    ConstantBuffer<SVODebugDrawSettings> settings = ResourceDescriptorHeap[descriptor_indices.settings.x];
    RWStructuredBuffer<uint> node_list = ResourceDescriptorHeap[descriptor_indices.node_list.x];
    DrawIndexedIndirectCommand command;
    // The edges of VOXEL_DEBUG_CUBE_INDICES, or the faces of VOXEL_DEBUG_CUBE_SOLID_INDICES.
    command.index_count = settings.style == 0 ? 24 : 36;
    if (settings.depth + 1 >= svo_info.max_depth) {
        command.instance_count = svo_info.leaves_count + svo_info.dynamic_leaves_count + 1;
    } else {
        command.instance_count = 1;
        if (settings.depth == 0) {
            [unroll] for (uint i = 0; i < 8; ++i) {
                if (tree_nodes[i].get_children_state() != SVONode::ChildrenState::EMPTY) {
                    node_list[command.instance_count - 1] = i;
                    ++command.instance_count;
                }
            }
        }
    }
    command.first_index = 0;
    command.vertex_offset = 0;
    command.first_instance = 0;
    indirect_draw[0] = command;
}
