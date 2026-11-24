/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_SVOGI_UTILS_SLANG
#define WBE_FILE_SVOGI_UTILS_SLANG

#define WBE_SVO_CONE_TRACE_MAX_STEPS 128

#include "svo/svo.slang"

struct SVOGIData {
    /**
     * @brief RGB color, alpha used for opacity.
     */
    float4 color;
    /**
     * @brief The average vector. The length encodes the variance: var = (1 - |average_vector|) / |average_vector|.
     */
    float3 average_vector;
};

/**
 * @brief Inject the occupancy of one leaf.
 * @details Writes 1 to the element of a leaf that svo.is_leaf_occupied() reports; nothing is written for an unoccupied or
 * superseded leaf, or for an index past svo.get_total_leaves().
 * @param p_svo_occupancy The SVO with its occupancy buffer, indexed by data index.
 * @param p_leaf_index Index of the leaf.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
void inject_svo_leaf_occupancy(SVOWriteBuffer<float> p_svo_occupancy, uint p_leaf_index, uint p_frame_slot) {
    SVO svo = p_svo_occupancy.svo;
    if (p_leaf_index < svo.get_total_leaves() && svo.is_leaf_occupied(p_leaf_index, p_frame_slot)) {
        p_svo_occupancy.write_data_at_index(svo.get_leaf(p_leaf_index).data_index, 1.0f, p_frame_slot);
    }
}

/**
 * @brief Inject the occupancy of the parent of one block of eight sibling leaves.
 * @details Writes the fraction of the parent's occupied sectors, from its children_data, to the parent's element, so it
 * does not depend on the leaves' elements. Nothing is written when the block lies past svo.get_total_leaves() or is a static
 * block superseded by a dynamic one, which still names its parent; only the referenced block writes a parent, so the
 * blocks may be injected concurrently.
 * @param p_svo_occupancy The SVO with its occupancy buffer, indexed by data index.
 * @param p_block_index Index of the block's first leaf; a multiple of eight.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
void inject_svo_leaf_parent_occupancy(SVOWriteBuffer<float> p_svo_occupancy, uint p_block_index, uint p_frame_slot) {
    SVO svo = p_svo_occupancy.svo;
    if (p_block_index >= svo.get_total_leaves()) {
        return;
    }
    SVONode parent = svo.get_node(uint(svo.get_leaf(p_block_index).parent_index));
    SVONode::ChildrenState expected_state =
        p_block_index >= svo.info.leaves_count ? SVONode::ChildrenState::LEAF_DYNAMIC : SVONode::ChildrenState::LEAF;
    if (parent.get_children_state() != expected_state || parent.get_children_index() != p_block_index) {
        return;
    }
    p_svo_occupancy.write_data_at_index(parent.data_index, float(parent.count_children_occupancy()) / 8.0f, p_frame_slot);
}

/**
 * @brief Propagate the occupancy of one block of eight sibling nodes to their parent.
 * @details Writes the average of the siblings' occupancy to the parent's element, reading a sibling's element only when it has
 * children, since only those were written; the others count as empty. Nothing is written when the block lies past
 * svo.get_total_nodes(), holds the root's sectors, is not at p_child_depth, or is a static block superseded by a dynamic
 * one, which still names its parent. Every sibling with children must already hold its occupancy, so the levels are
 * propagated from the deepest up, after the leaves' parents at depth svo.info.max_depth - 1 are written. Only the
 * referenced block writes a parent, so the blocks of one depth may be propagated concurrently.
 * @param p_svo_occupancy The SVO with its occupancy buffer, indexed by data index.
 * @param p_block_index Index of the block's first node; a multiple of eight.
 * @param p_child_depth Depth of the siblings to propagate, in [2, svo.info.max_depth - 1].
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
void propagate_svo_occupancy(SVOWriteBuffer<float> p_svo_occupancy, uint p_block_index, uint p_child_depth,
    uint p_frame_slot) {
    SVO svo = p_svo_occupancy.svo;
    if (p_block_index >= svo.get_total_nodes()) {
        return;
    }
    SVONode first_child = svo.get_node(p_block_index);
    // The root's sectors have no stored parent.
    if (!first_child.has_parent()) {
        return;
    }
    if (svo.get_node_depth(p_block_index, p_frame_slot) != p_child_depth) {
        return;
    }
    SVONode parent = svo.get_node(first_child.get_parent_index());
    SVONode::ChildrenState expected_state =
        p_block_index >= svo.info.node_count ? SVONode::ChildrenState::NODE_DYNAMIC : SVONode::ChildrenState::NODE;
    if (parent.get_children_state() != expected_state || parent.get_children_index() != p_block_index) {
        return;
    }
    float occupancy_sum = 0.0f;
    [unroll] for (uint i = 0; i < 8; ++i) {
        SVONode child = svo.get_node(p_block_index + i);
        SVONode::ChildrenState child_state = child.get_children_state();
        if (child_state == SVONode::ChildrenState::NODE || child_state == SVONode::ChildrenState::NODE_DYNAMIC ||
            child_state == SVONode::ChildrenState::LEAF || child_state == SVONode::ChildrenState::LEAF_DYNAMIC) {
            occupancy_sum += p_svo_occupancy.read_data_at_index(child.data_index, p_frame_slot);
        }
    }
    p_svo_occupancy.write_data_at_index(parent.data_index, occupancy_sum / 8.0f, p_frame_slot);
}

/**
 * @brief Number of sub-voxels in the brick of one SVO leaf, and the stride of a leaf's data index in a radiance key.
 */
static const uint SVOGI_SUBVOXEL_COUNT = SVO_BRICK_SIZE * SVO_BRICK_SIZE * SVO_BRICK_SIZE;

/**
 * @brief Locate the leaf sub-voxel that receives radiance injected at a world-space position.
 * @details Traces the position with svo.trace_data() and selects the sub-voxel with svo_pos_to_subvoxel_pos(). Positions
 * outside the SVO, which the trace would wrap into it, and positions whose trace ends at a node rather than an occupied
 * leaf, receive no radiance.
 * @param p_svo The SVO to inject into.
 * @param p_world_position Position in world space.
 * @param p_key Receives SVOGI_SUBVOXEL_COUNT * the leaf's data index + the sub-voxel index; 0 when nothing is located.
 * @param p_subvoxel_position Receives the position in the sub-voxel's local unit cube; 0 when nothing is located.
 * @return Whether an occupied leaf's sub-voxel was located.
 */
bool try_locate_svogi_subvoxel(SVO p_svo, float3 p_world_position, out uint p_key, out float3 p_subvoxel_position) {
    p_key = 0;
    p_subvoxel_position = float3(0.0);
    float3 svo_position = p_svo.get_svo_position(p_world_position);
    if (any(svo_position < 0.0) || any(svo_position >= 1.0)) {
        return false;
    }
    SVONodeTraceInfo trace_info;
    uint data_index = p_svo.trace_data(p_world_position, trace_info);
    if (trace_info.is_node()) {
        return false;
    }
    uint subvoxel_index;
    p_subvoxel_position = svo_pos_to_subvoxel_pos(trace_info.svo_position, subvoxel_index);
    p_key = SVOGI_SUBVOXEL_COUNT * data_index + subvoxel_index;
    return true;
}

/**
 * @brief The radiance vector a flux sample adds to its sub-voxel.
 * @details The flux is treated as a small virtual point light at its position in the sub-voxel, so each row is that
 * position scaled by one color channel of the flux.
 * @param p_subvoxel_position Position in the sub-voxel's local unit cube, from try_locate_svogi_subvoxel().
 * @param p_flux RGB flux of the sample.
 * @return The rows (ir, ig, ib): p_subvoxel_position scaled by p_flux.r, p_flux.g, and p_flux.b.
 */
float3x3 make_svogi_subvoxel_radiance(float3 p_subvoxel_position, float3 p_flux) {
    return float3x3(p_subvoxel_position * p_flux.r, p_subvoxel_position * p_flux.g, p_subvoxel_position * p_flux.b);
}

// TODO(5): Add propagate_svo_radiance(), mirroring propagate_svo_occupancy() above: average the eight siblings' radiance
// into the parent, with the same depth, dynamic/static block, and empty-child checks. Leaves' parents are written
// first from the 27 subvoxels of each leaf (as inject_svo_leaf_parent_occupancy() does for occupancy). Weighting by
// occupancy keeps empty children from darkening the average. Add its entry shader and svogi.manifest.lua resource.

struct Cone {
    float3 start_pos;
    float3 dir;
    float cone_angle_tanhalf;
    float max_distance;
};

float cone_tracing<T>(SVOReadBuffer<float> p_occupancy, StructuredBuffer<T> p_data, Cone p_cone, float p_beta = 0.5) {
    float3 svo_position = p_occupancy.svo.get_svo_position(p_cone.start_pos);
    float t = 0.0;
    T accumulated = T();
    for (uint i = 0; i < WBE_SVO_CONE_TRACE_MAX_STEPS; ++i) {
        // TODO(7): Implement the march: choose the depth whose voxel size matches the cone diameter at t
        // (2 * t * cone_angle_tanhalf), sample occupancy and radiance at that depth (interpolating between depths), and
        // composite front to back (accumulated += (1 - alpha) * occupancy * radiance; alpha += (1 - alpha) * occupancy).
        // Stop when alpha saturates, t exceeds max_distance, or the position leaves the SVO. Return the accumulated
        // value (the function currently returns nothing, and its float return type should probably be T).
    }
}

#endif
