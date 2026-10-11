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
#ifndef WBE_FILE_RT_VOXEL_DEBUG_VIEW_VK_HH
#define WBE_FILE_RT_VOXEL_DEBUG_VIEW_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/unique.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "i_voxel_render_feature.hh"
#include <atomic>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTVoxelDebugViewVK
 * @brief Records a retained debug task only while one of a range of voxel debug views is selected.
 * @details The wrapped task's accesses are reported every frame. Neither copyable nor movable; confined to the rendering
 * thread. The borrowed debug-mode state must outlive graph execution.
 */
class RTVoxelDebugViewVK final : public IRenderTaskVK {
public:
    RTVoxelDebugViewVK() = delete;
    /**
     * @brief Retains the task and borrows the debug-mode state.
     * @param p_task Debug task owned by the wrapper.
     * @param p_mode Selected debug view, written by other threads.
     * @param p_first_mode First mode of the inclusive range recording the task.
     * @param p_last_mode Last mode of the inclusive range recording the task.
     * @throws std::runtime_error If the task or mode state is null, or the range is empty.
     */
    RTVoxelDebugViewVK(Unique<IRenderTaskVK> p_task,
        const std::atomic<VoxelDebugViewMode>* p_mode,
        VoxelDebugViewMode p_first_mode,
        VoxelDebugViewMode p_last_mode);
    virtual ~RTVoxelDebugViewVK() override = default;
    RTVoxelDebugViewVK(const RTVoxelDebugViewVK&) = delete;
    RTVoxelDebugViewVK(RTVoxelDebugViewVK&&) noexcept = delete;
    RTVoxelDebugViewVK& operator=(const RTVoxelDebugViewVK&) = delete;
    RTVoxelDebugViewVK& operator=(RTVoxelDebugViewVK&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }
    virtual void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
    }
    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
    }
    /**
     * @brief Records the task when the selected mode lies in the task's range.
     */
    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    Unique<IRenderTaskVK> task;
    const std::atomic<VoxelDebugViewMode>* mode;
    VoxelDebugViewMode first_mode;
    VoxelDebugViewMode last_mode;
};

} // namespace WhiteBirdEngine

#endif
