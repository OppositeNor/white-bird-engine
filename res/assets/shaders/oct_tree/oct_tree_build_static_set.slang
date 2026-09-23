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
#ifndef WBE_OCT_TREE_BUILD_SLANG
#define WBE_OCT_TREE_BUILD_SLANG

#include "utils/utils.slang"
#include "oct_tree/oct_tree_utils.slang"

/**
 * @brief The input data from the vertex buffer
 */
struct VSInput {
    /**
     * @brief The position of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 position;
};

/**
 * @brief Vertex shader output / fragment shader input for the deferred G-buffer pass.
 */
struct VSOutput {
    /**
     * @brief Clip-space position.
     */
    float4 position       : SV_Position;
    /**
     * @brief Position in SVO space (0, 0 is the corner of the oct tree's region, and values inside the
     * oct tree are ranged [0, 1)^3)
     */
    float3 svo_position : TEXCOORD0;
};

struct OctTreeDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_info")]
    uint2 oct_tree_info;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_node_buffer")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_leaves_buffer")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_deferred_list_ping")]
    uint2 deferred_list;

    [WBE_DESCRIPTOR("storage_buffer", "node_data")]
    uint2 node_data;
    StaticSetDescriptorIndices static_set;
};

[[vk::push_constant]]
ConstantBuffer<OctTreeDescriptorIndices> descriptor_indices;

/**
 * @brief Transform a static-set vertex for the oct tree.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_set.instances.
 * @return Vertex output in clip space.
 */
[shader("vertex")]
VSOutput vertex_main(VSInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    RWStructuredBuffer<OctTreeInfo> oct_tree_info_buffer = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    OctTreeInfo oct_tree_info = oct_tree_info_buffer[0];
    float4x4 proj_matrix = oct_tree_info.matrices[WBE_OCT_TREE_VIEW_INDEX];
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[descriptor_indices.static_set.instances.x];
    StructuredBuffer<NodeData> nodes = ResourceDescriptorHeap[descriptor_indices.static_set.nodes.x];
    StructuredBuffer<SubmeshData> submeshes = ResourceDescriptorHeap[descriptor_indices.static_set.submeshes.x];

    InstanceData instance = instances[p_instance_id];
    NodeData node = nodes[instance.node_index];
    SubmeshData submesh = submeshes[instance.submesh_index];
    float4 world_position = mul(node.transform, float4(p_input.position.xyz, 1.0));
    float3x3 direction_transform = (float3x3)node.transform;

    VSOutput output;
    output.position = software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(proj_matrix), world_position);
    output.svo_position = (world_position.xyz - oct_tree_info.origin) / oct_tree_info.width;
    return output;
}

[shader("fragment")]
void fragment_main(VSOutput p_input) : SV_Target0 {
    RWStructuredBuffer<OctTreeInfo> oct_tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    RWStructuredBuffer<OctTreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    RWStructuredBuffer<OctTreeLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    RWStructuredBuffer<OctTreeDeferredListEntry> deferred_list = ResourceDescriptorHeap[descriptor_indices.deferred_list.x];
    // Trace to the bottom-most node
    OctTreeNodeTraceInfo trace_info;
    trace_info.node_index = 0;
    trace_info.curr_depth = 0;
    trace_info.remain_depth = oct_tree_info[0].max_depth;
    trace_info.svo_position = p_input.svo_position;
    // Try to allocate the node.
    if (!try_voxel_allocate(tree_nodes, tree_leaves, oct_tree_info, trace_info)) {
        // If allocation failed, push this to the deferred list.
        uint new_task_index;
        InterlockedAdd(oct_tree_info[0].deferred_task_count, 1, new_task_index);
        deferred_list[new_task_index].unfinished_trace_info = trace_info;
    }
}

#endif
