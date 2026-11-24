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

// Variants: WBE_SVO_DEBUG_DRAW_SOLID draws each voxel's shaded faces, shrunk by SOLID_VOXEL_SCALE so the borders between
// voxels show, instead of its edges; WBE_SVO_DEBUG_DRAW_OCCUPANCY colors each voxel by its injected occupancy.

struct SVODebugDrawDescIndices {
    [wbe::desc("uniform_buffer", "svo_info")]
    DescriptorHandle<ConstantBuffer<SVOInfo>> svo_info;
    [wbe::desc("storage_buffer", "svo_node_buffer", "read")]
    DescriptorHandle<StructuredBuffer<SVONode>> tree_nodes;
    [wbe::desc("storage_buffer", "svo_leaf_buffer", "read")]
    DescriptorHandle<StructuredBuffer<SVOLeaf>> tree_leaves;
    [wbe::desc("uniform_buffer", "svo_debug_draw_settings")]
    DescriptorHandle<ConstantBuffer<SVODebugDrawSettings>> settings;
    [wbe::desc("storage_buffer", "svo_debug_draw_node_list", "read")]
    DescriptorHandle<StructuredBuffer<uint>> node_list;
#if defined(WBE_SVO_DEBUG_DRAW_OCCUPANCY)
    [wbe::desc("storage_buffer", "svogi_occupancy_buffer", "read")]
    DescriptorHandle<StructuredBuffer<float>> occupancy;
#endif

    [wbe::desc("uniform_buffer", "gbuffer_camera_info")]
    DescriptorHandle<ConstantBuffer<CameraInfo>> camera_info;
    [wbe::push_constant("frame_slot")]
    uint shader_debug_frame_slot;
};

/**
 * @brief Vertex output for SVO debug draws.
 */
struct SVODebugDrawVertexOutput {
    /**
     * @brief Clip-space vertex position.
     */
    float4 position : SV_Position;
    /**
     * @brief Voxel color, shaded per face in the solid variants.
     */
    float4 color : COLOR0;
};

[vk::push_constant]
ConstantBuffer<SVODebugDrawDescIndices> di;

// The edge length of a solid voxel relative to its cell.
static const float SOLID_VOXEL_SCALE = 0.85;

/**
 * @brief A vertex outside the clip volume, which drops its primitive.
 */
SVODebugDrawVertexOutput discarded_vertex() {
    SVODebugDrawVertexOutput output;
    output.position = float4(2.0, 2.0, 2.0, 1.0);
    output.color = float4(0.0);
    return output;
}

/**
 * @brief The outward normal of a solid cube face, ordered as in VOXEL_DEBUG_CUBE_SOLID_INDICES: -x, +x, -y, +y, -z, +z.
 */
float3 face_normal(uint p_face) {
    float3 normal = float3(0.0);
    normal[p_face / 2] = (p_face % 2) == 0 ? -1.0 : 1.0;
    return normal;
}

/**
 * @brief Produces one of the corners of a leaf voxel, a node at the drawn depth, or the SVO bounds without a vertex buffer;
 * see svo_debug_draw_setup for the instance layout of each depth. A wireframe index is the corner; a solid one is
 * face * 8 + corner. The bounds are drawn only as edges.
 */
[shader("vertex")]
SVODebugDrawVertexOutput vertex_main(uint p_vertex_id : SV_VulkanVertexID, uint p_instance_id : SV_VulkanInstanceID) {
    ConstantBuffer<SVOInfo> svo_info = di.svo_info;
    StructuredBuffer<SVONode> tree_nodes = di.tree_nodes;
    StructuredBuffer<SVOLeaf> tree_leaves = di.tree_leaves;
    ConstantBuffer<CameraInfo> camera_info = di.camera_info;
    ConstantBuffer<SVODebugDrawSettings> settings = di.settings;
    StructuredBuffer<uint> node_list = di.node_list;
    uint frame_slot = di.shader_debug_frame_slot;
    SVO svo = SVO(tree_nodes, tree_leaves, svo_info, true);

    uint corner_index = p_vertex_id & 7;
    float3 corner = float3((corner_index >> 2) & 1, (corner_index >> 1) & 1, corner_index & 1);
    bool draws_leaves = settings.depth + 1 >= svo_info.max_depth;
    bool is_bounds = draws_leaves ? p_instance_id == svo.get_total_leaves() : p_instance_id == 0;
    float3 cell_origin;
    float cell_width;
    uint data_index;
    if (is_bounds) {
#if defined(WBE_SVO_DEBUG_DRAW_SOLID)
        return discarded_vertex();
#endif
        cell_origin = svo_info.origin;
        cell_width = svo_info.width;
        data_index = 0;
    }
    else if (!draws_leaves) {
        uint node_index = node_list[p_instance_id - 1];
        uint node_depth;
        uint3 node_grid_position = svo.get_node_grid_position(node_index, node_depth, frame_slot);
        cell_width = svo_info.width / float(1u << node_depth);
        cell_origin = svo_info.origin + float3(node_grid_position) * cell_width;
        data_index = svo.get_node(node_index).data_index;
    }
    else {
        if (!svo.is_leaf_occupied(p_instance_id, frame_slot)) {
            return discarded_vertex();
        }
        uint3 leaf_grid_position = svo.get_leaf_grid_position(p_instance_id, frame_slot);
        cell_width = svo_info.width / float(1u << svo_info.max_depth);
        cell_origin = svo_info.origin + float3(leaf_grid_position) * cell_width;
        data_index = svo.get_leaf(p_instance_id).data_index;
    }
#if defined(WBE_SVO_DEBUG_DRAW_SOLID)
    float3 world_position = cell_origin + (0.5 + (corner - 0.5) * SOLID_VOXEL_SCALE) * cell_width;
#else
    float3 world_position = cell_origin + corner * cell_width;
#endif
    float3 color = float3(1.0);
#if defined(WBE_SVO_DEBUG_DRAW_OCCUPANCY)
    if (!is_bounds) {
        StructuredBuffer<float> occupancy = di.occupancy;
        // The gray level is the occupancy: black for empty, white for full.
        color = float3(saturate(occupancy[data_index]));
    }
#endif
#if defined(WBE_SVO_DEBUG_DRAW_SOLID) && !defined(WBE_SVO_DEBUG_DRAW_OCCUPANCY)
    // Faces facing the fixed light direction are brighter, so the cube's shape reads without textures. The occupancy view
    // keeps every face at its exact gray level.
    float3 light_direction = normalize(float3(0.4, 1.0, 0.25));
    color *= 0.55 + 0.45 * dot(face_normal(p_vertex_id / 8), light_direction);
#endif
    float4 view_position =
        software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(camera_info.view), float4(world_position, 1.0));
    SVODebugDrawVertexOutput output;
    output.position = software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(camera_info.proj), view_position);
    output.color = is_bounds ? float4(1.0, 0.0, 0.0, 1.0) : float4(color, 1.0);
    return output;
}

/**
 * @brief Colors each debug voxel.
 */
[shader("fragment")]
float4 fragment_main(SVODebugDrawVertexOutput p_input) : SV_Target0 {
    return p_input.color;
}
