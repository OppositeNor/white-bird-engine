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
#include "rt_voxel_debug_view_vk.hh"
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

RTVoxelDebugViewVK::RTVoxelDebugViewVK(Unique<IRenderTaskVK> p_task,
    const std::atomic<VoxelDebugViewMode>* p_mode,
    VoxelDebugViewMode p_first_mode,
    VoxelDebugViewMode p_last_mode)
    : task(std::move(p_task)), mode(p_mode), first_mode(p_first_mode), last_mode(p_last_mode) {
    if (task == MEM_NULL || mode == nullptr) {
        throw std::runtime_error("Failed to create the voxel debug-view task: task or mode state is null.");
    }
    if (first_mode > last_mode) {
        throw std::runtime_error("Failed to create the voxel debug-view task: mode range is invalid.");
    }
}

void RTVoxelDebugViewVK::record(VkCommandBuffer p_command_buffer, double p_delta) {
    VoxelDebugViewMode current_mode = mode->load(std::memory_order_relaxed);
    if (current_mode >= first_mode && current_mode <= last_mode) {
        task->record(p_command_buffer, p_delta);
    }
}

} // namespace WhiteBirdEngine
