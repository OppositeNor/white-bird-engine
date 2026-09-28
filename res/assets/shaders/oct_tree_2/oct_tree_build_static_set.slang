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
#include "oct_tree_2/oct_tree_utils.slang"
#ifdef WBE_DYNAMIC_BUILD
#include "oct_tree/oct_tree_dynamic_build.slang"
#endif

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
 * @brief Transformed triangle vertices for voxel coverage generation.
 */
struct VSOutput {
    /**
     * @brief Normalized position before the geometry shader selects a projection.
     */
    float4 position       : SV_Position;
    /**
     * @brief Position in SVO space (0, 0 is the corner of the oct tree's region, and values inside the
     * oct tree are ranged [0, 1)^3)
     */
    float3 svo_position : TEXCOORD0;
    /**
     * @brief Number of cells along each oct-tree axis.
     */
    nointerpolation uint grid_extent : TEXCOORD1;
};

/**
 * @brief A projected candidate rectangle carrying the original triangle in voxel-grid coordinates.
 */
struct GSOutput {
    /**
     * @brief Clip-space position of the candidate rectangle.
     */
    float4 position : SV_Position;
    /**
     * @brief First triangle vertex, constant across the rectangle.
     */
    nointerpolation float3 triangle_a : TEXCOORD0;
    /**
     * @brief Second triangle vertex, constant across the rectangle.
     */
    nointerpolation float3 triangle_b : TEXCOORD1;
    /**
     * @brief Third triangle vertex, constant across the rectangle.
     */
    nointerpolation float3 triangle_c : TEXCOORD2;
    /**
     * @brief Original grid axis used as depth for this triangle's projection.
     */
    nointerpolation uint depth_axis : TEXCOORD3;
};

// Closed-cell overlap includes surfaces on cell boundaries. This tolerance is in voxel units.
static const float VOXEL_COVERAGE_EPSILON = 0.0001;

/**
 * @brief Reflected descriptor-heap bindings for this construction stage.
 */
struct OctTreeDescriptorIndices {
    /**
     * @brief Descriptor-heap reference to oct_tree_info.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_info")]
    uint2 oct_tree_info;
    /**
     * @brief Descriptor-heap reference to oct_tree_node_buffer.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_node_buffer")]
    uint2 tree_nodes;
    /**
     * @brief Descriptor-heap reference to oct_tree_pending_children.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_pending_children")]
    uint2 pending_children;
    /**
     * @brief Descriptor-heap reference to oct_tree_voxel_claims.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_voxel_claims")]
    uint2 voxel_claims;
    /**
     * @brief Descriptor-heap reference to oct_tree_leaves_buffer.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_leaves_buffer")]
    uint2 tree_leaves;
#ifdef WBE_DYNAMIC_BUILD
    /**
     * @brief Descriptor-heap reference to oct_tree_dynamic_nodes.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_dynamic_nodes")]
    uint2 dynamic_nodes;
    /**
     * @brief Descriptor-heap reference to oct_tree_dynamic_leaves.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_dynamic_leaves")]
    uint2 dynamic_leaves;
#endif

    /**
     * @brief Descriptor-heap reference to oct_tree_deferred_list_ping.
     */
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_deferred_list_ping")]
    uint2 deferred_list;
    /**
     * @brief Renderer frame slot used for shader validation reports.
     */
    [WBE_DEFAULT_PUSH("frame_slot")]
    uint shader_debug_frame_slot;

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
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[descriptor_indices.static_set.instances.x];
    StructuredBuffer<NodeData> nodes = ResourceDescriptorHeap[descriptor_indices.static_set.nodes.x];
    StructuredBuffer<SubmeshData> submeshes = ResourceDescriptorHeap[descriptor_indices.static_set.submeshes.x];

    InstanceData instance = instances[p_instance_id];
    NodeData node = nodes[instance.node_index];
    SubmeshData submesh = submeshes[instance.submesh_index];
    float4 world_position = mul(node.transform, float4(p_input.position.xyz, 1.0));
    float3x3 direction_transform = (float3x3)node.transform;

    VSOutput output;
    output.svo_position = (world_position.xyz - oct_tree_info.origin) / oct_tree_info.width;
    output.position = float4(output.svo_position * 2.0 - 1.0, 1.0);
    output.grid_extent = 1u << oct_tree_info.max_depth;
    return output;
}

/**
 * @brief Emits every candidate pixel column in the triangle's dominant projection, including subpixel triangles.
 * @details Rasterize a cell-aligned rectangle instead of extrapolating attributes outside conservatively rasterized triangles.
 * The fragment stage rejects rectangle cells that do not actually intersect the original triangle.
 */
[shader("geometry")]
[maxvertexcount(4)]
void geometry_main(triangle VSOutput p_input[3], inout TriangleStream<GSOutput> p_stream) {
    float extent = float(p_input[0].grid_extent);
    float3 a = p_input[0].svo_position * extent;
    float3 b = p_input[1].svo_position * extent;
    float3 c = p_input[2].svo_position * extent;
    float3 normal = abs(cross(b - a, c - a));
    uint dominant_axis = normal.x >= normal.y && normal.x >= normal.z ? 0 : (normal.y >= normal.z ? 1 : 2);
    if (normal[dominant_axis] == 0.0) {
        return;
    }
    uint horizontal_axis = dominant_axis == 0 ? 1 : 0;
    uint vertical_axis = dominant_axis == 2 ? 1 : 2;
    float3 lower = max(floor(min(a, min(b, c)) - VOXEL_COVERAGE_EPSILON), float3(0.0));
    float3 upper = min(floor(max(a, max(b, c)) + VOXEL_COVERAGE_EPSILON), float3(extent - 1.0));
    if (any(lower > upper)) {
        return;
    }
    float2 rectangle_min = float2(lower[horizontal_axis], lower[vertical_axis]);
    float2 rectangle_max = float2(upper[horizontal_axis], upper[vertical_axis]) + 1.0;
    for (uint i = 0; i < 4; ++i) {
        float2 corner = float2((i & 1) ? rectangle_max.x : rectangle_min.x,
            (i & 2) ? rectangle_max.y : rectangle_min.y);
        GSOutput output;
        output.position = float4(corner / extent * 2.0 - 1.0, 0.5, 1.0);
        output.triangle_a = a;
        output.triangle_b = b;
        output.triangle_c = c;
        output.depth_axis = dominant_axis;
        p_stream.Append(output);
    }
    p_stream.RestartStrip();
}

/**
 * @brief Tests overlap of a centered voxel and triangle on one separating axis; zero-length axes cannot separate them.
 */
bool triangle_voxel_axis_overlaps(float3 p_axis, float3 p_a, float3 p_b, float3 p_c) {
    float3 projections = float3(dot(p_axis, p_a), dot(p_axis, p_b), dot(p_axis, p_c));
    float radius = dot(abs(p_axis), float3(0.5 + VOXEL_COVERAGE_EPSILON));
    return min(projections.x, min(projections.y, projections.z)) <= radius &&
        max(projections.x, max(projections.y, projections.z)) >= -radius;
}

/**
 * @brief Tests triangle/voxel overlap on the three box axes, triangle normal, and nine edge cross-product axes.
 */
bool triangle_overlaps_voxel(float3 p_a, float3 p_b, float3 p_c, float3 p_center) {
    float3 a = p_a - p_center;
    float3 b = p_b - p_center;
    float3 c = p_c - p_center;
    float half_width = 0.5 + VOXEL_COVERAGE_EPSILON;
    if (any(min(a, min(b, c)) > half_width) || any(max(a, max(b, c)) < -half_width)) {
        return false;
    }
    float3 edges[3] = {b - a, c - b, a - c};
    if (!triangle_voxel_axis_overlaps(cross(edges[0], edges[1]), a, b, c)) {
        return false;
    }
    for (uint i = 0; i < 3; ++i) {
        float3 edge = edges[i];
        if (!triangle_voxel_axis_overlaps(float3(0.0, edge.z, -edge.y), a, b, c) ||
            !triangle_voxel_axis_overlaps(float3(-edge.z, 0.0, edge.x), a, b, c) ||
            !triangle_voxel_axis_overlaps(float3(edge.y, -edge.x, 0.0), a, b, c)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Allocates an intersected cell, retaining unfinished traversal in the deferred queue on contention.
 */
void allocate_covered_voxel(float3 p_svo_position) {
    RWStructuredBuffer<OctTreeInfo> oct_tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    // Construction currently records occupancy only: all triangle hits on the same voxel are equivalent.
    // Keep exactly one retry chain per voxel so the phase boundary cannot overflow the deferred queue with duplicates.
    uint extent = 1u << oct_tree_info[0].max_depth;
    uint3 voxel = uint3(p_svo_position * float(extent));
    uint voxel_index = voxel.x + extent * (voxel.y + extent * voxel.z);
    RWStructuredBuffer<uint> voxel_claims = ResourceDescriptorHeap[descriptor_indices.voxel_claims.x];
    uint bit = 1u << (voxel_index & 31u);
    uint original_claims;
    InterlockedOr(voxel_claims[voxel_index >> 5], bit, original_claims);
    if ((original_claims & bit) != 0) {
        return;
    }
    RWStructuredBuffer<OctTreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    RWStructuredBuffer<uint> pending_children = ResourceDescriptorHeap[descriptor_indices.pending_children.x];
    RWStructuredBuffer<OctTreeLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    RWStructuredBuffer<OctTreeDeferredListEntry> deferred_list = ResourceDescriptorHeap[descriptor_indices.deferred_list.x];
    // Trace to the bottom-most node
    OctTreeNodeTraceInfo trace_info;
    trace_info.node_index = get_oct_tree_node_subregion(p_svo_position);
    trace_info.curr_depth = 1;
    trace_info.remain_depth = int(oct_tree_info[0].max_depth) - 1;
    trace_info.svo_position = frac(p_svo_position * 2.0);
    // Try to allocate the node.
    #ifdef WBE_DYNAMIC_BUILD
    RWStructuredBuffer<OctTreeNode> dynamic_nodes = ResourceDescriptorHeap[descriptor_indices.dynamic_nodes.x];
    RWStructuredBuffer<OctTreeDynamicLeafBlock> dynamic_leaves = ResourceDescriptorHeap[descriptor_indices.dynamic_leaves.x];
    if (!try_dynamic_voxel_allocate(tree_nodes, tree_leaves, dynamic_nodes, dynamic_leaves, oct_tree_info, trace_info, descriptor_indices.shader_debug_frame_slot)) {
#else
    if (!try_voxel_allocate(tree_nodes, tree_leaves, pending_children, oct_tree_info, trace_info, descriptor_indices.shader_debug_frame_slot)) {
#endif
        // If allocation failed, push this to the deferred list.
        uint new_task_index;
        InterlockedAdd(oct_tree_info[0].deferred_task_count_ping, 1, new_task_index);
        WBE_SHADER_DEBUG_ASSERT(new_task_index < oct_tree_info[0].max_deferred_task_count,
            ShaderDebugError::OCT_TREE_DEFERRED_TASK_INDEX_OVERFLOW, descriptor_indices.shader_debug_frame_slot);
        deferred_list[new_task_index].unfinished_trace_info = trace_info;
    }
}

/**
 * @brief Allocates only cells intersecting the original triangle, including every depth cell touched by the pixel column.
 */
[shader("fragment")]
void fragment_main(GSOutput p_input) {
    // Helper invocations cannot publish storage writes and receive undefined atomic results.
    // Keep them out of allocation retry loops; this shader does not need derivatives.
    if (IsHelperLane()) {
        return;
    }
    StructuredBuffer<OctTreeInfo> tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    float extent = float(1u << tree_info[0].max_depth);
    uint depth_axis = p_input.depth_axis;
    uint horizontal_axis = depth_axis == 0 ? 1 : 0;
    uint vertical_axis = depth_axis == 2 ? 1 : 2;
    float3 center = float3(0.0);
    center[horizontal_axis] = floor(p_input.position.x) + 0.5;
    center[vertical_axis] = floor(p_input.position.y) + 0.5;
    // Other than the voxel in the center, we also check if the triangle intersects any other voxels
    // in the depth direction.
    float3 a = p_input.triangle_a;
    float3 b = p_input.triangle_b;
    float3 c = p_input.triangle_c;
    float3 normal = cross(b - a, c - a);
    float center_depth = a[depth_axis] -
        (normal[horizontal_axis] * (center[horizontal_axis] - a[horizontal_axis]) +
            normal[vertical_axis] * (center[vertical_axis] - a[vertical_axis])) / normal[depth_axis];
    float depth_radius = (abs(normal[horizontal_axis]) + abs(normal[vertical_axis])) /
        abs(normal[depth_axis]) * (0.5 + VOXEL_COVERAGE_EPSILON);
    int first_depth = int(max(0.0, floor(center_depth - depth_radius - VOXEL_COVERAGE_EPSILON)));
    int last_depth = int(min(extent - 1.0, floor(center_depth + depth_radius + VOXEL_COVERAGE_EPSILON)));
    for (int depth = first_depth; depth <= last_depth; ++depth) {
        center[depth_axis] = float(depth) + 0.5;
        if (triangle_overlaps_voxel(a, b, c, center)) {
            allocate_covered_voxel(center / extent);
        }
    }
}

#endif
