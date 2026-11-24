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
/**
 * @file octree_kernel.slang
 * @brief The octree debug compute kernels, whose entry points the ACP generates.
 */
#ifndef WBE_FILE_OCTREE_KERNEL_SLANG
#define WBE_FILE_OCTREE_KERNEL_SLANG

#include "utils/utils.slang"
#include "octree/octree_debug.slang"

groupshared uint octree_debug_gather_leaves_subgrid_mask[OCTREE_DEBUG_SUBGRID_MASK_WORD_COUNT];

/**
 * @brief Build the leaf view's indirect draws from the leaf level of both octree textures.
 * @details The leaf grid is split into OCTREE_DEBUG_SUBGRID_EXTENT subgrids, and each workgroup gathers one: every
 * invocation tests one OCTREE_DEBUG_SUBGRID_INVOCATION_LEAF_EXTENT block of leaves, and the workgroup writes the subgrid's
 * occupancy mask and one indexed indirect draw command. The command draws one instance per occupied leaf, starting at
 * instance subgrid_index * OCTREE_DEBUG_SUBGRID_LEAF_COUNT, so the vertex shader recovers the subgrid and the leaf's rank in
 * the mask from the instance index. The command after the last subgrid draws the octree's bounds. The dispatch must cover
 * every subgrid with one workgroup each. The pass also empties the node view's list and instance count before the node
 * gather. The workgroup size mirrors OCTREE_DEBUG_SUBGRID_INVOCATION_EXTENT.
 * @param p_group_id The workgroup of the invocation, which selects its subgrid.
 * @param p_group_thread_id The invocation's coordinate in its workgroup, which selects its block of leaves.
 * @param p_group_index The invocation's flattened index in its workgroup.
 * @param p_octree_info The octree's configuration, in its first element.
 * @param p_static_levels The static octree texture with every mip level.
 * @param p_dynamic_levels The dynamic octree texture with every mip level.
 * @param p_leaf_masks Receives the occupancy mask of every subgrid.
 * @param p_indirect_draw Receives the indirect draw of every subgrid, then the bounds draw.
 * @param p_node_list The node view's list, whose count is reset.
 * @param p_node_indirect_draw The node view's indirect draw, whose instance count is reset.
 */
[wbe::kernel(4, 4, 8)]
void octree_debug_gather_leaves(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
    uint p_group_index : SV_GroupIndex,
    [wbe::bind("octree_info")] StructuredBuffer<OctreeInfo> p_octree_info,
    [wbe::bind("octree_static")] Texture3D<uint> p_static_levels,
    [wbe::bind("octree_dynamic")] Texture3D<uint> p_dynamic_levels,
    [wbe::bind("octree_debug_leaf_masks", "write")] RWStructuredBuffer<OctreeDebugSubgridMask> p_leaf_masks,
    [wbe::bind("octree_debug_draw_indirect_buffer", "write")] RWStructuredBuffer<DrawIndexedIndirectCommand> p_indirect_draw,
    [wbe::bind("octree_debug_node_list", "write")] RWStructuredBuffer<uint> p_node_list,
    [wbe::bind("octree_debug_node_draw_indirect_buffer", "write")]
    RWStructuredBuffer<DrawIndexedIndirectCommand> p_node_indirect_draw) {
    OctreeInfo info = p_octree_info[0];
    uint3 grid_extent = get_octree_debug_subgrid_grid_extent(info.max_depth);
    uint subgrid_index = get_octree_debug_subgrid_index(p_group_id, grid_extent);

    if (p_group_index < OCTREE_DEBUG_SUBGRID_MASK_WORD_COUNT) {
        octree_debug_gather_leaves_subgrid_mask[p_group_index] = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    uint3 first_local_leaf = p_group_thread_id * OCTREE_DEBUG_SUBGRID_INVOCATION_LEAF_EXTENT;
    uint3 first_voxel = p_group_id * OCTREE_DEBUG_SUBGRID_EXTENT + first_local_leaf;
    // The invocation's block is one texel of mip level 1, which is occupied exactly when one of the block's leaves is, so
    // the test skips the leaf loads of empty blocks.
    if (get_octree_occupancy(p_static_levels, p_dynamic_levels, true, 1, first_voxel >> 1) != 0) {
        for (uint z = 0; z < OCTREE_DEBUG_SUBGRID_INVOCATION_LEAF_EXTENT.z; ++z) {
            for (uint y = 0; y < OCTREE_DEBUG_SUBGRID_INVOCATION_LEAF_EXTENT.y; ++y) {
                for (uint x = 0; x < OCTREE_DEBUG_SUBGRID_INVOCATION_LEAF_EXTENT.x; ++x) {
                    uint3 offset = uint3(x, y, z);
                    if (get_octree_occupancy(p_static_levels, p_dynamic_levels, true, 0, first_voxel + offset) == 0) {
                        continue;
                    }
                    uint bit = get_octree_debug_subgrid_local_index(first_local_leaf + offset);
                    InterlockedOr(octree_debug_gather_leaves_subgrid_mask[bit / 32], 1u << (bit % 32));
                }
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();
    if (p_group_index != 0) {
        return;
    }
    OctreeDebugSubgridMask mask;
    uint occupied_count = 0;
    for (uint word_index = 0; word_index < OCTREE_DEBUG_SUBGRID_MASK_WORD_COUNT; ++word_index) {
        mask.words[word_index / 4][word_index % 4] = octree_debug_gather_leaves_subgrid_mask[word_index];
        occupied_count += countbits(octree_debug_gather_leaves_subgrid_mask[word_index]);
    }
    p_leaf_masks[subgrid_index] = mask;
    DrawIndexedIndirectCommand command;
    command.index_count = OCTREE_DEBUG_CUBE_INDEX_COUNT;
    command.instance_count = occupied_count;
    command.first_index = 0;
    command.vertex_offset = 0;
    command.first_instance = subgrid_index * OCTREE_DEBUG_SUBGRID_LEAF_COUNT;
    p_indirect_draw[subgrid_index] = command;
    if (subgrid_index != 0) {
        return;
    }
    // The first workgroup also writes the bounds command and empties the node view.
    uint subgrid_count = grid_extent.x * grid_extent.y * grid_extent.z;
    command.instance_count = 1;
    command.first_instance = subgrid_count * OCTREE_DEBUG_SUBGRID_LEAF_COUNT;
    p_indirect_draw[subgrid_count] = command;
    p_node_list[OCTREE_DEBUG_NODE_LIST_COUNT_INDEX] = 0;
    command.instance_count = 0;
    command.first_instance = 0;
    p_node_indirect_draw[0] = command;
}

/**
 * @brief Append one node to the node view if the static scene or the dynamic meshes occupy it.
 * @param p_info The octree's configuration.
 * @param p_static_levels The static octree texture with every mip level.
 * @param p_dynamic_levels The dynamic octree texture with every mip level.
 * @param p_node_list The node view's list, which receives the node.
 * @param p_node_indirect_draw The node view's indirect draw, whose instance count counts the node.
 * @param p_level The node's level.
 * @param p_node The node's coordinate at its level.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
void octree_debug_gather_node(OctreeInfo p_info, Texture3D<uint> p_static_levels, Texture3D<uint> p_dynamic_levels,
    RWStructuredBuffer<uint> p_node_list, RWStructuredBuffer<DrawIndexedIndirectCommand> p_node_indirect_draw,
    uint p_level, uint3 p_node, uint p_frame_slot) {
    if (get_octree_occupancy(p_static_levels, p_dynamic_levels, true, p_level, p_node) == 0) {
        return;
    }
    uint entry_index;
    InterlockedAdd(p_node_list[OCTREE_DEBUG_NODE_LIST_COUNT_INDEX], 1, entry_index);
    WBE_SHADER_DEBUG_ASSERT(entry_index < p_info.max_debug_node_count,
        ShaderDebugError::OCTREE_DEBUG_NODE_LIST_OVERFLOW, p_frame_slot);
    if (entry_index >= p_info.max_debug_node_count) {
        return;
    }
    p_node_list[OCTREE_DEBUG_NODE_LIST_ENTRY_OFFSET + entry_index] = to_octree_debug_node_entry(p_level, p_node);
    InterlockedAdd(p_node_indirect_draw[0].instance_count, 1);
}

/**
 * @brief Append every occupied node between the leaves and the root to the node view.
 * @details Each invocation owns one texel of the gather level: it scans every node of its subtree below that level, and the
 * nodes above it whose lowest-corner descendant at the gather level is its texel, so every node is visited exactly once.
 * The dispatch must cover the gather level, which is clamped to max_depth.
 * @param p_dispatch_thread_id The invocation's texel of the gather level.
 * @param p_octree_info The octree's configuration, in its first element.
 * @param p_static_levels The static octree texture with every mip level.
 * @param p_dynamic_levels The dynamic octree texture with every mip level.
 * @param p_node_list The node view's list, which receives the nodes.
 * @param p_node_indirect_draw The node view's indirect draw, whose instance count counts the nodes.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(4, 4, 4)]
void octree_debug_gather_nodes(uint3 p_dispatch_thread_id : SV_DispatchThreadID,
    [wbe::bind("octree_info")] StructuredBuffer<OctreeInfo> p_octree_info,
    [wbe::bind("octree_static")] Texture3D<uint> p_static_levels,
    [wbe::bind("octree_dynamic")] Texture3D<uint> p_dynamic_levels,
    [wbe::bind("octree_debug_node_list")] RWStructuredBuffer<uint> p_node_list,
    [wbe::bind("octree_debug_node_draw_indirect_buffer")]
    RWStructuredBuffer<DrawIndexedIndirectCommand> p_node_indirect_draw,
    [wbe::push("frame_slot")] uint p_frame_slot) {
    OctreeInfo info = p_octree_info[0];
    uint gather_level = min(OCTREE_DEBUG_GATHER_LEVEL, info.max_depth);
    uint3 texel = p_dispatch_thread_id;
    if (any(texel >= (1u << (info.max_depth - gather_level)))) {
        return;
    }
    // The root is drawn as the octree's bounds by the leaf view, so the node view stops below it.
    for (uint level = max(gather_level, 1); level < info.max_depth; ++level) {
        uint span_mask = (1u << (level - gather_level)) - 1;
        if (all((texel & span_mask) == 0)) {
            octree_debug_gather_node(info, p_static_levels, p_dynamic_levels, p_node_list, p_node_indirect_draw, level,
                texel >> (level - gather_level), p_frame_slot);
        }
    }
    if (get_octree_occupancy(p_static_levels, p_dynamic_levels, true, gather_level, texel) == 0) {
        // Nothing below an empty texel is occupied.
        return;
    }
    for (uint level = 1; level < gather_level; ++level) {
        uint span = 1u << (gather_level - level);
        uint3 first_node = texel * span;
        for (uint z = 0; z < span; ++z) {
            for (uint y = 0; y < span; ++y) {
                for (uint x = 0; x < span; ++x) {
                    octree_debug_gather_node(info, p_static_levels, p_dynamic_levels, p_node_list, p_node_indirect_draw,
                        level, first_node + uint3(x, y, z), p_frame_slot);
                }
            }
        }
    }
}

#endif
