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
#include "svo/svo.slang"
#include "svo/svo_utils.slang"

struct DescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer", "svo_pre_deferred_debug_info_buffer")]
    uint2 svo_info;
    [WBE_DESCRIPTOR("storage_buffer", "svo_node_buffer", "read")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "svo_leaf_buffer", "read")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("storage_image", "occupancy", "write")]
    uint2 occupancy;
    [WBE_PUSH_CONSTANT("frame_slot")]
    uint shader_debug_frame_slot;
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> descriptor_indices;

[shader("compute")]
[numthreads(128, 1, 1)]
void main(uint p_group_id : SV_GroupID, uint p_group_thread_id : SV_GroupThreadID, uint p_thread_id : SV_DispatchThreadID) {
    ConstantBuffer<SVOInfo> svo_info = ResourceDescriptorHeap[descriptor_indices.svo_info.x];
    StructuredBuffer<SVONode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    StructuredBuffer<SVOLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    RWTexture3D<float> occupancy = ResourceDescriptorHeap[descriptor_indices.occupancy.x];
    uint frame_slot = descriptor_indices.shader_debug_frame_slot;

    // Injection writes the SVO's normal distribution function.
    SVOWriteImage<float> svo_occupancy =
        SVOWriteImage<float>(SVO(tree_nodes, tree_leaves, svo_info, true), occupancy);
    bool is_leaf_occupied = svo_occupancy.svo.is_leaf_occupied(p_thread_id, frame_slot);
    if (is_leaf_occupied) {
        svo_occupancy.write_leaf_data(p_thread_id, 1.0, descriptor_indices.shader_debug_frame_slot);
    }
    GroupMemoryBarrierWithGroupSync();
    // The thread with the top left corner is responsible for the write of the parent node.
    // We use p_group_thread_id < 16 instead of p_group_thread_id % 8 to try to concentrate as much thread in the same subgroup
    // possible.
    if (p_group_thread_id < 16) {
        // We extract the thread on the first corner, which is responsible for updating the state of its parent.
        return;
    }
    uint responsible_index = p_group_id * 128 + p_group_thread_id * 8;
    // Because we have the workgroup grouped in 128 invocations, which is a multiple of 8, meaning we could safely assume that
    // all the 8 leaves shares the same parent node are allocated in the same workgroup.
    uint parent_index = svo_occupancy.svo.get_leaf(responsible_index).parent_index;
    SVONode parent = svo_occupancy.svo.get_node(parent_index);
    // We already have the info of the first children, so we 
    // TODO
}
