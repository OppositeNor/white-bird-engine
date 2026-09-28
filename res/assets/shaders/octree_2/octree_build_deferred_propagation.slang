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
#include "octree_2/octree_utils.slang"
#ifdef WBE_DYNAMIC_BUILD
#include "octree/octree_dynamic_build.slang"
#endif

/**
 * @brief Reflected descriptor-heap bindings for this construction stage.
 */
struct OctreeDescriptorIndices {
    /**
     * @brief Descriptor-heap reference to octree_info.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_info")]
    uint2 octree_info;
    /**
     * @brief Descriptor-heap reference to octree_node_buffer.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_node_buffer")]
    uint2 tree_nodes;
    /**
     * @brief Descriptor-heap reference to octree_pending_children.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_pending_children")]
    uint2 pending_children;
    /**
     * @brief Descriptor-heap reference to octree_leaves_buffer.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_leaves_buffer")]
    uint2 tree_leaves;
#ifdef WBE_DYNAMIC_BUILD
    /**
     * @brief Descriptor-heap reference to octree_dynamic_nodes.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_dynamic_nodes")]
    uint2 dynamic_nodes;
    /**
     * @brief Descriptor-heap reference to octree_dynamic_leaves.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_dynamic_leaves")]
    uint2 dynamic_leaves;
#endif

#ifdef WBE_USE_PING_WRITE
    /**
     * @brief Descriptor-heap reference to octree_deferred_list_pong.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_deferred_list_pong")]
    uint2 deferred_list_read;
    /**
     * @brief Descriptor-heap reference to octree_deferred_list_ping.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_deferred_list_ping")]
    uint2 deferred_list_write;
#else
    /**
     * @brief Descriptor-heap reference to octree_deferred_list_ping.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_deferred_list_ping")]
    uint2 deferred_list_read;
    /**
     * @brief Descriptor-heap reference to octree_deferred_list_pong.
     */
    [WBE_DESCRIPTOR("storage_buffer", "octree_deferred_list_pong")]
    uint2 deferred_list_write;
#endif
    /**
     * @brief Renderer frame slot used for shader validation reports.
     */
    [WBE_DEFAULT_PUSH("frame_slot")]
    uint shader_debug_frame_slot;
};

[[vk::push_constant]]
ConstantBuffer<OctreeDescriptorIndices> descriptor_indices;

/**
 * @brief Advances each retained voxel once and queues unfinished work in the opposite list.
 */
[shader("compute")]
[numthreads(128, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    RWStructuredBuffer<OctreeInfo> octree_info = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
#ifdef WBE_USE_PING_WRITE
    uint deferred_task_count = octree_info[0].deferred_task_count_pong;
#else
    uint deferred_task_count = octree_info[0].deferred_task_count_ping;
#endif
    if (p_dispatch_thread_id.x >= deferred_task_count) {
        // Extra thread in the group. No work to do.
        return;
    }
    RWStructuredBuffer<OctreeNode> tree_nodes = ResourceDescriptorHeap[descriptor_indices.tree_nodes.x];
    RWStructuredBuffer<uint> pending_children = ResourceDescriptorHeap[descriptor_indices.pending_children.x];
    RWStructuredBuffer<OctreeLeaf> tree_leaves = ResourceDescriptorHeap[descriptor_indices.tree_leaves.x];
    StructuredBuffer<OctreeDeferredListEntry> deferred_list_read = ResourceDescriptorHeap[descriptor_indices.deferred_list_read.x];
    RWStructuredBuffer<OctreeDeferredListEntry> deferred_list_write = ResourceDescriptorHeap[descriptor_indices.deferred_list_write.x];
    OctreeDeferredListEntry task = deferred_list_read[p_dispatch_thread_id.x];

    OctreeNodeTraceInfo trace_info = task.unfinished_trace_info;
    // Try to allocate the node.
    #ifdef WBE_DYNAMIC_BUILD
    RWStructuredBuffer<OctreeNode> dynamic_nodes = ResourceDescriptorHeap[descriptor_indices.dynamic_nodes.x];
    RWStructuredBuffer<OctreeDynamicLeafBlock> dynamic_leaves = ResourceDescriptorHeap[descriptor_indices.dynamic_leaves.x];
    if (!try_dynamic_voxel_allocate(tree_nodes, tree_leaves, dynamic_nodes, dynamic_leaves, octree_info, trace_info, descriptor_indices.shader_debug_frame_slot)) {
#else
    if (!try_voxel_allocate(tree_nodes, tree_leaves, pending_children, octree_info, trace_info, descriptor_indices.shader_debug_frame_slot)) {
#endif
        // If allocation failed, push this to the deferred list.
        uint new_task_index;
#ifdef WBE_USE_PING_WRITE
        InterlockedAdd(octree_info[0].deferred_task_count_ping, 1, new_task_index);
#else
        InterlockedAdd(octree_info[0].deferred_task_count_pong, 1, new_task_index);
#endif
        WBE_SHADER_DEBUG_ASSERT(new_task_index < octree_info[0].max_deferred_task_count,
            ShaderDebugError::OCTREE_DEFERRED_TASK_INDEX_OVERFLOW, descriptor_indices.shader_debug_frame_slot);
        deferred_list_write[new_task_index].unfinished_trace_info = trace_info;
    }
}
