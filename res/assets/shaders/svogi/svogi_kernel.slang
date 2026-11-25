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
 * @file svogi_kernel.slang
 * @brief The SVOGI compute kernels, whose entry points the ACP generates; their building blocks are in svogi_utils.slang.
 */
#ifndef WBE_FILE_SVOGI_KERNEL_SLANG
#define WBE_FILE_SVOGI_KERNEL_SLANG

#include "svogi/svogi_utils.slang"
#include "prefix_scan/prefix_scan_utils.slang"
#include "utils/utils.slang"
#include "utils/gpu_vector.slang"
#include "light/spot_light_utils.slang"

/**
 * @brief Inject the occupancy of every leaf and of every leaf's parent, one invocation per leaf.
 * @details Each invocation injects its leaf with inject_svo_leaf_occupancy(); after a workgroup barrier, the first 32
 * invocations each inject the parent of one block of eight leaves with inject_svo_leaf_parent_occupancy(). The workgroup
 * size, 256, is a multiple of eight, so the eight leaves sharing a parent are injected by one workgroup, and the low
 * invocations keep the parent writes in as few subgroups as possible. The workgroups are dispatched as a 2D grid, flattened
 * row by row, so the leaf capacity may need more workgroups than one dimension guarantees.
 * @param p_group_id The workgroup of the invocation in the 2D dispatch grid.
 * @param p_group_thread_id The invocation's index in its workgroup.
 * @param p_svo_info Spatial configuration of the SVO.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_tree_leaves The SVO's leaves.
 * @param p_atlas The occupancy buffer, indexed by data index.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 */
[wbe::kernel(256)]
void inject_svo_occupancy(uint2 p_group_id : SV_GroupID, uint p_group_thread_id : SV_GroupThreadID,
                          [wbe::bind("svo_info")] ConstantBuffer<SVOInfo> p_svo_info,
                          [wbe::bind("svo_node_buffer")] StructuredBuffer<SVONode> p_tree_nodes,
                          [wbe::bind("svo_leaf_buffer")] StructuredBuffer<SVOLeaf> p_tree_leaves,
                          [wbe::bind("atlas", "write")] RWStructuredBuffer<float> p_atlas,
                          [wbe::push("frame_slot")] uint p_frame_slot) {
    uint group_index = p_group_id.y * WorkgroupCount().x + p_group_id.x;
    uint thread_id = group_index * 256 + p_group_thread_id;
    var svo_occupancy = SVOWriteBuffer<float>(SVO(p_tree_nodes, p_tree_leaves, p_svo_info, true), p_atlas);
    inject_svo_leaf_occupancy(svo_occupancy, thread_id, p_frame_slot);
    GroupMemoryBarrierWithGroupSync();
    if (p_group_thread_id >= 32) {
        return;
    }
    inject_svo_leaf_parent_occupancy(svo_occupancy, group_index * 256 + p_group_thread_id * 8, p_frame_slot);
}

/**
 * @brief Propagate the occupancy one level up the tree, from the nodes at p_child_depth to their parents.
 * @details The occupancy injection writes the leaves and their parents at depth max_depth - 1, so this kernel is
 * dispatched once for each child depth from max_depth - 1 down to 2, in that order, with one invocation for each block of
 * eight nodes. Nodes are allocated in blocks of eight siblings, so the invocation of a block is responsible for their
 * parent.
 * @param p_thread_id The invocation's index in the dispatch, which is the index of its block.
 * @param p_svo_info Spatial configuration of the SVO.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_tree_leaves The SVO's leaves.
 * @param p_atlas The occupancy buffer, indexed by data index.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 * @param p_child_depth Depth of the child nodes this dispatch reads; their parents, one level up, are written.
 */
[wbe::kernel(64)]
void propagate_svogi_occupancy(uint p_thread_id : SV_DispatchThreadID,
                               [wbe::bind("svo_info")] ConstantBuffer<SVOInfo> p_svo_info,
                               [wbe::bind("svo_node_buffer")] StructuredBuffer<SVONode> p_tree_nodes,
                               [wbe::bind("svo_leaf_buffer")] StructuredBuffer<SVOLeaf> p_tree_leaves,
                               [wbe::bind("atlas")] RWStructuredBuffer<float> p_atlas,
                               [wbe::push("frame_slot")] uint p_frame_slot,
                               [wbe::push("child_depth")] uint p_child_depth) {
    var svo_occupancy = SVOWriteBuffer<float>(SVO(p_tree_nodes, p_tree_leaves, p_svo_info, true), p_atlas);
    propagate_svo_occupancy(svo_occupancy, p_thread_id * 8, p_child_depth, p_frame_slot);
}

// The staged radiance of inject_svogi_radiance(), in float3x3, where each row represents ir, ig, ib.
groupshared float3x3 inject_svogi_radiance_local_staged_buffer[256 * WBE_MAX_SPOT_LIGHT_COUNT];
// The (key, local index) of each staged radiance of inject_svogi_radiance().
groupshared uint2 inject_svogi_radiance_local_index_buffer[256 * WBE_MAX_SPOT_LIGHT_COUNT];
// The number of radiances inject_svogi_radiance() has staged in the workgroup.
groupshared uint inject_svogi_radiance_buffer_tracker;
// The first index of the workgroup's radiances in the global buffers.
groupshared uint inject_svogi_radiance_global_index_start;

/**
 * @brief Inject the flux of every spot light's reflective shadow-map texel into the leaf sub-voxel it lies in.
 * @details Each invocation reads one texel of every spot light, at its z-ordered position in the 16x16 workgroup. The flux
 * is treated as small virtual point lights. The workgroup stages its samples in groupshared memory, reserves one range of
 * the global index and staged buffers, and writes them there.
 * @param p_group_id The workgroup of the invocation.
 * @param p_group_thread_id The invocation's position in its workgroup.
 * @param p_svo_info Spatial configuration of the SVO.
 * @param p_tree_nodes The SVO's nodes.
 * @param p_tree_leaves The SVO's leaves.
 * @param p_index_buffer The GPU vector receiving each sample's (key, index).
 * @param p_staged_buffer The buffer receiving each sample's radiance.
 * @param p_light_infos The spot light information.
 * @param p_frame_slot Renderer frame slot receiving shader-debug errors.
 * @param p_lights The descriptor-heap resources of each spot light.
 */
[wbe::kernel(16, 16)]
void inject_svogi_radiance(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
                           [wbe::bind("svo_info")] ConstantBuffer<SVOInfo> p_svo_info,
                           [wbe::bind("svo_node_buffer")] StructuredBuffer<SVONode> p_tree_nodes,
                           [wbe::bind("svo_leaf_buffer")] StructuredBuffer<SVOLeaf> p_tree_leaves,
                           [wbe::bind("index_buffer", "write")] RWByteAddressBuffer p_index_buffer,
                           [wbe::bind("staged_buffer", "write")] RWByteAddressBuffer p_staged_buffer,
                           [wbe::bind("spot_light_infos")] StructuredBuffer<SpotLightInfoBuffer> p_light_infos,
                           [wbe::push("frame_slot")] uint p_frame_slot,
                           [wbe::push_data] LightParamDescIndices p_lights[WBE_MAX_SPOT_LIGHT_COUNT]) {
    uint2 dispatch_thread_id = z_order_swizzle_16x16(p_group_id.xy, p_group_thread_id.xy);
    var index_buffer = GPUVector<uint2>(p_index_buffer, p_frame_slot);
    var staged_buffer = GPUVector<SegmentedWrapper<float3x3>>(p_staged_buffer, p_frame_slot);

    SVO svo = SVO(p_tree_nodes, p_tree_leaves, p_svo_info, true);
    uint spot_light_count = get_spot_light_count(p_light_infos);
    float2 uv = float2(dispatch_thread_id) / float2(uint2(16, 16) * WorkgroupCount().xy);
    if (all(p_group_thread_id.xy == uint2(0, 0))) {
        inject_svogi_radiance_buffer_tracker = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    // We first gather all the voxels each invocation in this workgroup is going to write into, perform a sort on them, and
    // sum up the ones that are in the same voxel.
    for (uint spot_light_index = 0; spot_light_index < spot_light_count; ++spot_light_index) {
        SpotLight spot_light = SpotLight(p_light_infos, p_lights[spot_light_index], spot_light_index);
        // Read the position and the flux from the same shadow-map texel; the reflective shadow map shares its extent.
        uint2 shadow_size = spot_light.get_shadow_size();
        uint2 texel = min(uint2(uv * float2(shadow_size)), shadow_size - 1);
        float3 position = spot_light.shadow_texel_world_position(uv);
        // Only occupied leaves inside the SVO receive radiance.
        uint key;
        float3 subvoxel_position;
        if (!try_locate_svogi_subvoxel(svo, position, key, subvoxel_position)) {
            continue;
        }
        float3 color = spot_light.rsm_flux.Load(int3(int2(texel), 0)).rgb;
        uint index;
        InterlockedAdd(inject_svogi_radiance_buffer_tracker, 1, index);
        // We treat the flux as small VPLs. We inject those VPLs into the voxels.
        inject_svogi_radiance_local_index_buffer[index] = uint2(key, index);
        inject_svogi_radiance_local_staged_buffer[index] = make_svogi_subvoxel_radiance(subvoxel_position, color);
    }
    GroupMemoryBarrierWithGroupSync();
    if (all(p_group_thread_id.xy == uint2(0, 0))) {
        inject_svogi_radiance_global_index_start = index_buffer.atomic_expand(inject_svogi_radiance_buffer_tracker);
        // There might be racing conditions; however, since we atomically expanded index_buffer, it will end up >= the expected
        // size, so it's fine. Eventually, one of the workgroup would set this to the largest expected size, which is the one
        // we'd expect.
        staged_buffer.set_size(max(staged_buffer.size(), index_buffer.size()));
    }
    GroupMemoryBarrierWithGroupSync();
    // We have a maximum of 256 * WBE_MAX_SPOT_LIGHT_COUNT indices to write. Each invocation is responsible of WBE_MAX_SPOT_LIGHT_COUNT.
    uint thread_id = p_group_thread_id.xy.flatten(16);
    [unroll] for (uint i = 0; i < WBE_MAX_SPOT_LIGHT_COUNT; ++i) {
        uint index = thread_id * 4 + i;
        if (index >= inject_svogi_radiance_buffer_tracker) {
            return;
        }
        index_buffer.set(inject_svogi_radiance_global_index_start + index, inject_svogi_radiance_local_index_buffer[index]);
        staged_buffer.set(inject_svogi_radiance_global_index_start + index, {inject_svogi_radiance_local_staged_buffer[index], 0});
    }
}

[wbe::kernel(256)]
void radiance_duplication_mark(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID,
                               [wbe::bind("staged_buffer", "write")] RWByteAddressBuffer p_staged_buffer,
                               [wbe::push("frame_slot")] uint p_frame_slot) {
    GPUVector<SegmentedWrapper<float3x3>> staged_buffer = GPUVector<SegmentedWrapper<float3x3>>(p_staged_buffer, p_frame_slot);
    uint index = p_group_id.x * 256 + p_group_thread_id.x;
    if (index > staged_buffer.size()) {
        return;
    }
    if (index == 0) {
        staged_buffer.set(0, {float3x3(0.0), false});
        return;
    }
    let current = staged_buffer.get(index);
    let previous = staged_buffer.get(index - 1);
    staged_buffer.set(index, {current.value, any(current.value != previous.value) ? 1 : 0});
}
#endif
