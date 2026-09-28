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
#ifndef WBE_OCTREE_BUILD_SLANG
#define WBE_OCTREE_BUILD_SLANG

#include "utils/utils.slang"
#include "octree/octree_utils.slang"

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
     * @brief Position in SVO space (0, 0 is the corner of the octree's region, and values inside the
     * octree are ranged [0, 1)^3)
     */
    float3 svo_position : TEXCOORD0;
    /**
     * @brief Number of cells along each octree axis.
     */
    nointerpolation uint grid_extent : TEXCOORD1;
};

/**
 * @brief A conservatively enlarged projected triangle carrying the original triangle in voxel-grid coordinates.
 */
struct GSOutput {
    /**
     * @brief Clip-space position of the enlarged triangle.
     */
    float4 position : SV_Position;
    /**
     * @brief First triangle vertex, constant across the enlarged triangle.
     */
    nointerpolation float3 triangle_a : TEXCOORD0;
    /**
     * @brief Second triangle vertex, constant across the enlarged triangle.
     */
    nointerpolation float3 triangle_b : TEXCOORD1;
    /**
     * @brief Third triangle vertex, constant across the enlarged triangle.
     */
    nointerpolation float3 triangle_c : TEXCOORD2;
    /**
     * @brief Original grid axis used as depth for this triangle's projection.
     */
    nointerpolation uint depth_axis : TEXCOORD3;
    /**
     * @brief Inclusive cell range of the original triangle: (horizontal min, vertical min, horizontal max, vertical max).
     */
    nointerpolation uint4 cell_bounds : TEXCOORD4;
    /**
     * @brief Inclusive cell range of the original triangle along the depth axis: (min, max).
     */
    nointerpolation uint2 depth_bounds : TEXCOORD5;
};

// Closed-cell overlap includes surfaces on cell boundaries. This tolerance is in voxel units.
static const float VOXEL_COVERAGE_EPSILON = 0.0001;

struct OctreeDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "octree_info")]
    uint2 octree_info;
    [WBE_DESCRIPTOR("storage_buffer", "octree_node_buffer")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer", "octree_leaves_buffer")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("storage_buffer", "octree_deferred_list_ping")]
    uint2 deferred_list;
    [WBE_DEFAULT_PUSH("frame_slot")]
    uint shader_debug_frame_slot;

    StaticSetDescriptorIndices static_set;
};

[[vk::push_constant]]
ConstantBuffer<OctreeDescriptorIndices> descriptor_indices;

/**
 * @brief Transform a static-set vertex for the octree.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_set.instances.
 * @return Vertex output in clip space.
 */
[shader("vertex")]
VSOutput vertex_main(VSInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    RWStructuredBuffer<OctreeInfo> octree_info_buffer = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
    OctreeInfo octree_info = octree_info_buffer[0];
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[descriptor_indices.static_set.instances.x];
    StructuredBuffer<NodeData> nodes = ResourceDescriptorHeap[descriptor_indices.static_set.nodes.x];
    StructuredBuffer<SubmeshData> submeshes = ResourceDescriptorHeap[descriptor_indices.static_set.submeshes.x];

    InstanceData instance = instances[p_instance_id];
    NodeData node = nodes[instance.node_index];
    SubmeshData submesh = submeshes[instance.submesh_index];
    float4 world_position = mul(node.transform, float4(p_input.position.xyz, 1.0));
    float3x3 direction_transform = (float3x3)node.transform;

    VSOutput output;
    output.svo_position = (world_position.xyz - octree_info.origin) / octree_info.width;
    output.position = float4(output.svo_position * 2.0 - 1.0, 1.0);
    output.grid_extent = 1u << octree_info.max_depth;
    return output;
}

/**
 * @brief Returns the homogeneous 2D line of one projected edge, pushed outward to cover every touched pixel.
 * @details The line is (n.x, n.y, d) with n pointing into the triangle, so inside points satisfy dot(n, p) + d >= 0.
 * A unit pixel square centered at p touches the edge's inner half-plane exactly when
 * dot(n, p) + d >= -0.5 * (|n.x| + |n.y|); adding that support distance to d turns "the square touches" into
 * "the pixel center is inside". p_orientation is the sign of the projected triangle's signed area.
 */
float3 get_enlarged_edge_line(float2 p_start, float2 p_end, float p_orientation) {
    float2 edge = p_end - p_start;
    float2 inward_normal = float2(-edge.y, edge.x) * p_orientation;
    float offset = dot(inward_normal, -p_start) + (0.5 + VOXEL_COVERAGE_EPSILON) * (abs(inward_normal.x) + abs(inward_normal.y));
    return float3(inward_normal, offset);
}

/**
 * @brief Emits the triangle's dominant projection enlarged by half a pixel along each edge normal.
 * @details Every pixel whose square touches the projected triangle has its center inside the enlarged triangle, including
 * pixels of subpixel triangles. The enlarged triangle's corners overshoot at acute angles, so the triangle's inclusive cell
 * bounds are passed for the fragment stage to clip against; it also rejects cells that do not intersect the original
 * triangle. Fragments use only the flat original vertices, never attributes interpolated across the enlarged triangle.
 */
[shader("geometry")]
[maxvertexcount(3)]
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
    float2 projected[3] = {
        float2(a[horizontal_axis], a[vertical_axis]),
        float2(b[horizontal_axis], b[vertical_axis]),
        float2(c[horizontal_axis], c[vertical_axis]),
    };
    float2 first_edge = projected[1] - projected[0];
    float2 second_edge = projected[2] - projected[0];
    // Nonzero: it equals the dominant normal component checked above, up to sign.
    float orientation = sign(first_edge.x * second_edge.y - first_edge.y * second_edge.x);
    float3 edge_lines[3];
    for (uint i = 0; i < 3; ++i) {
        edge_lines[i] = get_enlarged_edge_line(projected[i], projected[(i + 1) % 3], orientation);
    }
    uint4 cell_bounds = uint4(uint(lower[horizontal_axis]), uint(lower[vertical_axis]),
        uint(upper[horizontal_axis]), uint(upper[vertical_axis]));
    uint2 depth_bounds = uint2(uint(lower[dominant_axis]), uint(upper[dominant_axis]));
    for (uint i = 0; i < 3; ++i) {
        // Vertex i moves to the intersection of its two adjacent enlarged edges. Adjacent edges of a nondegenerate
        // triangle are never parallel, so the homogeneous weight is nonzero.
        float3 corner = cross(edge_lines[(i + 2) % 3], edge_lines[i]);
        GSOutput output;
        output.position = float4(corner.xy / corner.z / extent * 2.0 - 1.0, 0.5, 1.0);
        output.triangle_a = a;
        output.triangle_b = b;
        output.triangle_c = c;
        output.depth_axis = dominant_axis;
        output.cell_bounds = cell_bounds;
        output.depth_bounds = depth_bounds;
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
    RWStructuredBuffer<OctreeInfo> octree_info = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
    RWStructuredBuffer<OctreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    RWStructuredBuffer<OctreeLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    RWStructuredBuffer<OctreeDeferredListEntry> deferred_list = ResourceDescriptorHeap[descriptor_indices.deferred_list.x];
    // Trace to the bottom-most node
    OctreeNodeTraceInfo trace_info;
    trace_info.node_index = get_octree_node_subregion(p_svo_position);
    trace_info.curr_depth = 1;
    trace_info.remain_depth = int(octree_info[0].max_depth) - 1;
    trace_info.svo_position = frac(p_svo_position * 2.0);
    // Try to allocate the node.
    if (!try_voxel_allocate(tree_nodes, tree_leaves, octree_info, trace_info, descriptor_indices.shader_debug_frame_slot)) {
        // If allocation failed, push this to the deferred list.
        uint new_task_index;
        InterlockedAdd(octree_info[0].deferred_task_count_ping, 1, new_task_index);
        WBE_SHADER_DEBUG_ASSERT(new_task_index < octree_info[0].max_deferred_task_count,
            ShaderDebugError::OCTREE_DEFERRED_TASK_INDEX_OVERFLOW, descriptor_indices.shader_debug_frame_slot);
        deferred_list[new_task_index].unfinished_trace_info = trace_info;
    }
}

/**
 * @brief Allocates only cells intersecting the original triangle, including every depth cell touched by the pixel column.
 * @details Pixels outside the original triangle's cell bounds come from the enlarged triangle's overshooting corners and
 * are discarded before any overlap test.
 */
[shader("fragment")]
void fragment_main(GSOutput p_input) {
    // Helper invocations cannot publish storage writes and receive undefined atomic results.
    // Keep them out of allocation retry loops; this shader does not need derivatives.
    if (IsHelperLane()) {
        return;
    }
    // Pixel centers lie at cell + 0.5, so integer cell comparison has no boundary ambiguity.
    uint2 cell = uint2(p_input.position.xy);
    if (any(cell < p_input.cell_bounds.xy) || any(cell > p_input.cell_bounds.zw)) {
        return;
    }
    StructuredBuffer<OctreeInfo> tree_info = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
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
    // The triangle's own depth range also bounds the plane estimate, which overshoots near the triangle's edges.
    int first_depth = int(max(float(p_input.depth_bounds.x), floor(center_depth - depth_radius - VOXEL_COVERAGE_EPSILON)));
    int last_depth = int(min(float(p_input.depth_bounds.y), floor(center_depth + depth_radius + VOXEL_COVERAGE_EPSILON)));
    for (int depth = first_depth; depth <= last_depth; ++depth) {
        center[depth_axis] = float(depth) + 0.5;
        if (triangle_overlaps_voxel(a, b, c, center)) {
            allocate_covered_voxel(center / extent);
        }
    }
}

#endif
