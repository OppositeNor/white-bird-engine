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
#include "octree/octree_dynamic_read.slang"

struct OctreePreDeferredDebugDrawDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "octree_pre_deferred_debug_info_buffer", "read")]
    uint2 octree_info;
    [WBE_DESCRIPTOR("storage_buffer", "octree_node_buffer", "read")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "octree_leaves_buffer", "read")]
    uint2 tree_leaves;
    /**
     * @brief Descriptor-heap reference to octree_dynamic_nodes.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_dynamic_nodes", "read")]
    uint2 dynamic_nodes;
    /**
     * @brief Descriptor-heap reference to octree_dynamic_leaves.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_dynamic_leaves", "read")]
    uint2 dynamic_leaves;

    [WBE_DESCRIPTOR("storage_buffer", "octree_pre_deferred_debug_list_buffer", "read")]
    uint2 deferred_list;
    [WBE_DESCRIPTOR("uniform_buffer", "gbuffer_camera_info")]
    uint2 camera_info;
    [WBE_DEFAULT_PUSH("frame_slot")]
    uint shader_debug_frame_slot;
};

/**
 * @brief Vertex output for the pre-deferred octree debug wireframes.
 */
struct OctreePreDeferredDebugDrawVertexOutput {
    /**
     * @brief Clip-space vertex position.
     */
    float4 position : SV_Position;
    /**
     * @brief Wireframe color.
     */
    float4 color : COLOR0;
};

[[vk::push_constant]]
ConstantBuffer<OctreePreDeferredDebugDrawDescriptorIndices> descriptor_indices;

uint3 get_node_grid_position(uint p_node_index,
    uint p_depth,
    StructuredBuffer<OctreeNode> p_nodes,
    StructuredBuffer<OctreeNode> p_dynamic_nodes,
    OctreeInfo p_tree_info,
    uint p_frame_slot) {
    uint3 result = uint3(0);
    WBE_SHADER_DEBUG_ASSERT(p_node_index < p_tree_info.max_node_count + p_tree_info.dynamic_node_count,
        ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
    uint child_index = p_node_index;
    int parent_index = read_incremental_node(p_nodes, p_dynamic_nodes, p_tree_info, p_node_index).parent_index;
    uint traversed_depth = 0;
    for (; traversed_depth < p_depth && parent_index >= 0; ++traversed_depth) {
        WBE_SHADER_DEBUG_ASSERT(uint(parent_index) < p_tree_info.max_node_count + p_tree_info.dynamic_node_count,
            ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        OctreeNode parent = read_incremental_node(p_nodes, p_dynamic_nodes, p_tree_info, uint(parent_index));
        WBE_SHADER_DEBUG_ASSERT(child_index >= get_octree_children_index(parent) &&
            child_index - get_octree_children_index(parent) < 8,
            ShaderDebugError::OCTREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
        uint subregion = child_index % 8;
        result.x |= ((subregion >> 2) & 1) << traversed_depth;
        result.y |= ((subregion >> 1) & 1) << traversed_depth;
        result.z |= (subregion & 1) << traversed_depth;
        child_index = uint(parent_index);
        parent_index = parent.parent_index;
    }
    if (parent_index < 0) {
        WBE_SHADER_DEBUG_ASSERT(child_index < 8 && traversed_depth < 32,
            ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        // The remaining node is one of the eight sectors of the implicit root.
        result.x |= ((child_index >> 2) & 1) << traversed_depth;
        result.y |= ((child_index >> 1) & 1) << traversed_depth;
        result.z |= (child_index & 1) << traversed_depth;
        ++traversed_depth;
    }
    WBE_SHADER_DEBUG_ASSERT(parent_index < 0 && traversed_depth <= p_depth,
        ShaderDebugError::OCTREE_DEEPER_THAN_EXPECTED, p_frame_slot);
    WBE_SHADER_DEBUG_ASSERT(parent_index >= 0 || traversed_depth >= p_depth,
        ShaderDebugError::OCTREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
    return result;
}

uint3 get_leaf_grid_position(uint p_leaf_index,
    StructuredBuffer<OctreeNode> p_nodes,
    StructuredBuffer<OctreeNode> p_dynamic_nodes,
    StructuredBuffer<OctreeLeaf> p_leaves,
    StructuredBuffer<OctreeDynamicLeafBlock> p_dynamic_leaves,
    OctreeInfo p_tree_info,
    uint p_frame_slot) {
    WBE_SHADER_DEBUG_ASSERT(p_leaf_index < p_tree_info.leaves_count + p_tree_info.dynamic_leaves_count,
        ShaderDebugError::OCTREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
    uint child_index = p_leaf_index < p_tree_info.leaves_count ? p_leaf_index : p_leaf_index - p_tree_info.leaves_count;
    int parent_index = read_incremental_leaf(p_leaves, p_dynamic_leaves, p_tree_info, p_leaf_index).parent_index;
    uint3 result = uint3(0);
    uint traversed_depth = 0;
    for (; traversed_depth < p_tree_info.max_depth && parent_index >= 0; ++traversed_depth) {
        WBE_SHADER_DEBUG_ASSERT(uint(parent_index) < p_tree_info.max_node_count + p_tree_info.dynamic_node_count,
            ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        OctreeNode parent = read_incremental_node(p_nodes, p_dynamic_nodes, p_tree_info, uint(parent_index));
        WBE_SHADER_DEBUG_ASSERT(child_index >= get_octree_children_index(parent) &&
            child_index - get_octree_children_index(parent) < 8,
            ShaderDebugError::OCTREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
        uint subregion = child_index % 8;
        result.x |= ((subregion >> 2) & 1) << traversed_depth;
        result.y |= ((subregion >> 1) & 1) << traversed_depth;
        result.z |= (subregion & 1) << traversed_depth;
        child_index = uint(parent_index);
        parent_index = parent.parent_index;
    }
    if (parent_index < 0) {
        WBE_SHADER_DEBUG_ASSERT(child_index < 8 && traversed_depth < 32,
            ShaderDebugError::OCTREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        // The remaining node is one of the eight sectors of the implicit root.
        result.x |= ((child_index >> 2) & 1) << traversed_depth;
        result.y |= ((child_index >> 1) & 1) << traversed_depth;
        result.z |= (child_index & 1) << traversed_depth;
        ++traversed_depth;
    }
    WBE_SHADER_DEBUG_ASSERT(parent_index < 0 && traversed_depth <= p_tree_info.max_depth,
        ShaderDebugError::OCTREE_DEEPER_THAN_EXPECTED, p_frame_slot);
    WBE_SHADER_DEBUG_ASSERT(parent_index >= 0 || traversed_depth >= p_tree_info.max_depth,
        ShaderDebugError::OCTREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
    return result;
}

uint3 get_deferred_task_grid_position(OctreeDeferredListEntry p_task,
    StructuredBuffer<OctreeNode> p_nodes,
    StructuredBuffer<OctreeNode> p_dynamic_nodes,
    OctreeInfo p_tree_info,
    uint p_frame_slot) {
    OctreeNodeTraceInfo trace_info = p_task.unfinished_trace_info;
    WBE_SHADER_DEBUG_ASSERT(trace_info.curr_depth <= p_tree_info.max_depth,
        ShaderDebugError::OCTREE_DEEPER_THAN_EXPECTED, p_frame_slot);
    uint3 node_grid_position =
        get_node_grid_position(trace_info.node_index, trace_info.curr_depth, p_nodes, p_dynamic_nodes, p_tree_info, p_frame_slot);
    uint remaining_depth = p_tree_info.max_depth - trace_info.curr_depth;
    uint local_extent = 1u << remaining_depth;
    uint3 local_grid_position = min(uint3(trace_info.svo_position * float(local_extent)), uint3(local_extent - 1));
    return node_grid_position * local_extent + local_grid_position;
}

/**
 * @brief Draws captured leaves in white and pre-propagation deferred tasks in red.
 */
[shader("vertex")]
OctreePreDeferredDebugDrawVertexOutput vertex_main(uint p_vertex_id : SV_VulkanVertexID, uint p_instance_id : SV_VulkanInstanceID) {
    StructuredBuffer<OctreeInfo> octree_info_buffer = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
    StructuredBuffer<OctreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    StructuredBuffer<OctreeLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    StructuredBuffer<OctreeDeferredListEntry> deferred_list = ResourceDescriptorHeap[descriptor_indices.deferred_list.x];
    StructuredBuffer<OctreeNode> dynamic_nodes = ResourceDescriptorHeap[descriptor_indices.dynamic_nodes.x];
    StructuredBuffer<OctreeDynamicLeafBlock> dynamic_leaves = ResourceDescriptorHeap[descriptor_indices.dynamic_leaves.x];
    ConstantBuffer<CameraInfo> camera_info = ResourceDescriptorHeap[descriptor_indices.camera_info.x];
    OctreeInfo octree_info = octree_info_buffer[0];

    bool is_deferred_task = p_instance_id >= (octree_info.leaves_count + octree_info.dynamic_leaves_count);
    if (!is_deferred_task && !is_incremental_leaf_occupied(
            p_instance_id, tree_nodes, tree_leaves, dynamic_nodes, dynamic_leaves, octree_info, descriptor_indices.shader_debug_frame_slot)) {
        OctreePreDeferredDebugDrawVertexOutput empty_output;
        empty_output.position = float4(2.0, 2.0, 2.0, 1.0);
        empty_output.color = float4(0.0);
        return empty_output;
    }
    uint3 grid_position = is_deferred_task
        ? get_deferred_task_grid_position(
              deferred_list[p_instance_id - (octree_info.leaves_count + octree_info.dynamic_leaves_count)], tree_nodes, dynamic_nodes, octree_info, descriptor_indices.shader_debug_frame_slot)
        : get_leaf_grid_position(
              p_instance_id, tree_nodes, dynamic_nodes, tree_leaves, dynamic_leaves, octree_info, descriptor_indices.shader_debug_frame_slot);
    uint3 corner = uint3((p_vertex_id >> 2) & 1, (p_vertex_id >> 1) & 1, p_vertex_id & 1);
    float voxel_width = octree_info.width / float(1u << octree_info.max_depth);
    float3 world_position = octree_info.origin + float3(grid_position + corner) * voxel_width;
    float4 view_position =
        software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(camera_info.view), float4(world_position, 1.0));

    OctreePreDeferredDebugDrawVertexOutput output;
    output.position = software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(camera_info.proj), view_position);
    output.color = is_deferred_task ? float4(1.0, 0.0, 0.0, 1.0) : float4(1.0);
    return output;
}

/**
 * @brief Colors each debug wireframe.
 */
[shader("fragment")]
float4 fragment_main(OctreePreDeferredDebugDrawVertexOutput p_input) : SV_Target0 {
    return p_input.color;
}
