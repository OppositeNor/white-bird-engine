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

struct DrawIndexedIndirectCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

struct OctreePreDeferredDebugCaptureDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "octree_info", "read")]
    uint2 octree_info;
    [WBE_DESCRIPTOR("storage_buffer", "octree_deferred_list_ping", "read")]
    uint2 deferred_list;
    [WBE_DESCRIPTOR("storage_buffer", "octree_pre_deferred_debug_info_buffer")]
    uint2 captured_octree_info;
    [WBE_DESCRIPTOR("storage_buffer", "octree_pre_deferred_debug_list_buffer")]
    uint2 captured_deferred_list;
    [WBE_DESCRIPTOR("storage_buffer", "octree_pre_deferred_debug_draw_indirect_buffer")]
    uint2 indirect_draw;
};

[[vk::push_constant]]
ConstantBuffer<OctreePreDeferredDebugCaptureDescriptorIndices> descriptor_indices;

/**
 * @brief Captures the octree state produced by the raster passes before deferred propagation changes it.
 */
[shader("compute")]
[numthreads(128, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    StructuredBuffer<OctreeInfo> octree_info = ResourceDescriptorHeap[descriptor_indices.octree_info.x];
    StructuredBuffer<OctreeDeferredListEntry> deferred_list = ResourceDescriptorHeap[descriptor_indices.deferred_list.x];
    RWStructuredBuffer<OctreeInfo> captured_octree_info = ResourceDescriptorHeap[descriptor_indices.captured_octree_info.x];
    RWStructuredBuffer<OctreeDeferredListEntry> captured_deferred_list =
        ResourceDescriptorHeap[descriptor_indices.captured_deferred_list.x];
    RWStructuredBuffer<DrawIndexedIndirectCommand> indirect_draw = ResourceDescriptorHeap[descriptor_indices.indirect_draw.x];

    OctreeInfo info = octree_info[0];
    uint deferred_task_count = info.deferred_task_count_ping;
    if (p_dispatch_thread_id.x == 0) {
        captured_octree_info[0] = info;
        DrawIndexedIndirectCommand command;
        command.index_count = 36;
        command.instance_count = info.leaves_count + info.dynamic_leaves_count + deferred_task_count;
        command.first_index = 0;
        command.vertex_offset = 0;
        command.first_instance = 0;
        indirect_draw[0] = command;
    }
    if (p_dispatch_thread_id.x < deferred_task_count) {
        captured_deferred_list[p_dispatch_thread_id.x] = deferred_list[p_dispatch_thread_id.x];
    }
}
