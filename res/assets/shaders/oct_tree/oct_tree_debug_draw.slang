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
#include "oct_tree/oct_tree_utils.slang"

struct OctTreeDebugDrawDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_info")]
    uint2 oct_tree_info;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_node_buffer")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_leaves_buffer")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("uniform_buffer", "gbuffer_camera_info")]
    uint2 camera_info;
    [WBE_DEFAULT_PUSH("frame_slot")]
    uint shader_debug_frame_slot;
};

/**
 * @brief Vertex output for oct-tree debug wireframes.
 */
struct OctTreeDebugDrawVertexOutput {
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
ConstantBuffer<OctTreeDebugDrawDescriptorIndices> descriptor_indices;

/**
 * @brief Reconstructs one leaf's grid position by walking from the leaf to the root.
 * @param p_leaf_index Leaf-buffer index selected by the draw instance.
 * @param p_nodes Oct-tree node storage.
 * @param p_leaves Oct-tree leaf storage.
 * @param p_tree_info Tree bounds and maximum depth.
 * @param p_frame_slot Renderer frame slot whose report region receives validation errors.
 * @return Integer position of the leaf in the finest oct-tree grid.
 */
uint3 get_leaf_grid_position(uint p_leaf_index,
    StructuredBuffer<OctTreeNode> p_nodes,
    StructuredBuffer<OctTreeLeaf> p_leaves,
    OctTreeInfo p_tree_info,
    uint p_frame_slot) {
    uint3 result = uint3(0);
    WBE_SHADER_DEBUG_ASSERT(p_leaf_index < p_tree_info.max_leaves_count,
        ShaderDebugError::OCT_TREE_LEAF_INDEX_OVERFLOW, p_frame_slot);
    uint child_index = p_leaf_index;
    int parent_index = p_leaves[p_leaf_index].parent_index;
    uint traversed_depth = 0;
    for (; traversed_depth < p_tree_info.max_depth && parent_index >= 0; ++traversed_depth) {
        WBE_SHADER_DEBUG_ASSERT(uint(parent_index) < p_tree_info.max_node_count,
            ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        OctTreeNode parent = p_nodes[parent_index];
        WBE_SHADER_DEBUG_ASSERT(child_index >= get_oct_tree_children_index(parent) &&
            child_index - get_oct_tree_children_index(parent) < 8,
            ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
        uint subregion = child_index % 8;
        result.x |= ((subregion >> 2) & 1) << traversed_depth;
        result.y |= ((subregion >> 1) & 1) << traversed_depth;
        result.z |= (subregion & 1) << traversed_depth;
        child_index = uint(parent_index);
        parent_index = parent.parent_index;
    }
    if (parent_index < 0) {
        WBE_SHADER_DEBUG_ASSERT(child_index < 8 && traversed_depth < 32,
            ShaderDebugError::OCT_TREE_NODE_INDEX_OVERFLOW, p_frame_slot);
        // The remaining node is one of the eight sectors of the implicit root.
        result.x |= ((child_index >> 2) & 1) << traversed_depth;
        result.y |= ((child_index >> 1) & 1) << traversed_depth;
        result.z |= (child_index & 1) << traversed_depth;
        ++traversed_depth;
    }
    WBE_SHADER_DEBUG_ASSERT(parent_index < 0 && traversed_depth <= p_tree_info.max_depth,
        ShaderDebugError::OCT_TREE_DEEPER_THAN_EXPECTED, p_frame_slot);
    WBE_SHADER_DEBUG_ASSERT(parent_index >= 0 || traversed_depth >= p_tree_info.max_depth,
        ShaderDebugError::OCT_TREE_SHALLOWER_THAN_EXPECTED, p_frame_slot);
    return result;
}

/**
 * @brief Produces one of the eight corners of a leaf voxel or the oct-tree bounds without a vertex buffer.
 */
[shader("vertex")]
OctTreeDebugDrawVertexOutput vertex_main(uint p_vertex_id : SV_VulkanVertexID, uint p_instance_id : SV_VulkanInstanceID) {
    StructuredBuffer<OctTreeInfo> oct_tree_info_buffer = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    StructuredBuffer<OctTreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    StructuredBuffer<OctTreeLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    ConstantBuffer<CameraInfo> camera_info = ResourceDescriptorHeap[descriptor_indices.camera_info.x];
    OctTreeInfo oct_tree_info = oct_tree_info_buffer[0];

    uint3 corner = uint3((p_vertex_id >> 2) & 1, (p_vertex_id >> 1) & 1, p_vertex_id & 1);
    bool is_bounds = p_instance_id == oct_tree_info.leaves_count;
    float3 world_position;
    if (is_bounds) {
        world_position = oct_tree_info.origin + float3(corner) * oct_tree_info.width;
    }
    else {
        if (!is_oct_tree_leaf_occupied(
                p_instance_id, tree_nodes, tree_leaves, oct_tree_info, descriptor_indices.shader_debug_frame_slot)) {
            OctTreeDebugDrawVertexOutput empty_output;
            empty_output.position = float4(2.0, 2.0, 2.0, 1.0);
            empty_output.color = float4(0.0);
            return empty_output;
        }
        uint3 leaf_grid_position =
            get_leaf_grid_position(p_instance_id, tree_nodes, tree_leaves, oct_tree_info, descriptor_indices.shader_debug_frame_slot);
        float voxel_width = oct_tree_info.width / float(1u << oct_tree_info.max_depth);
        world_position = oct_tree_info.origin + float3(leaf_grid_position + corner) * voxel_width;
    }
    float4 view_position =
        software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(camera_info.view), float4(world_position, 1.0));
    OctTreeDebugDrawVertexOutput output;
    output.position = software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(camera_info.proj), view_position);
    output.color = is_bounds ? float4(1.0, 0.0, 0.0, 1.0) : float4(1.0);
    return output;
}

/**
 * @brief Colors each debug wireframe.
 */
[shader("fragment")]
float4 fragment_main(OctTreeDebugDrawVertexOutput p_input) : SV_Target0 {
    return p_input.color;
}
