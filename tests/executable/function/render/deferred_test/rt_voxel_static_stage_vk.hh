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
#ifndef WBE_FILE_RT_VOXEL_STATIC_STAGE_VK_HH
#define WBE_FILE_RT_VOXEL_STATIC_STAGE_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTVoxelStaticStageVK
 * @brief Records one stage of a static voxel construction until the construction is completed.
 * @details The completion flag is shared by every graph variant using the same voxel structure, so the static scene is built
 * once across graphs. The stage finishing the construction sets the flag after recording. The wrapped task's accesses are
 * reported every frame. Neither copyable nor movable; confined to the rendering thread.
 */
class RTVoxelStaticStageVK final : public IRenderTaskVK {
public:
    RTVoxelStaticStageVK() = delete;
    /**
     * @brief Wraps a stage of the static construction.
     * @param p_task Stage task owned by the wrapper; must not be null.
     * @param p_completed Completion flag shared by every stage of the construction; must not be null.
     * @param p_finish Whether recording this stage completes the construction.
     * @param p_completion_message Message logged when this stage completes the construction.
     */
    RTVoxelStaticStageVK(Unique<IRenderTaskVK> p_task, Ref<bool> p_completed, bool p_finish, std::string_view p_completion_message);
    virtual ~RTVoxelStaticStageVK() override = default;
    RTVoxelStaticStageVK(const RTVoxelStaticStageVK&) = delete;
    RTVoxelStaticStageVK(RTVoxelStaticStageVK&&) = delete;
    RTVoxelStaticStageVK& operator=(const RTVoxelStaticStageVK&) = delete;
    RTVoxelStaticStageVK& operator=(RTVoxelStaticStageVK&&) = delete;

    virtual QueueFamilyVK queue_family() const override {
        return task->queue_family();
    }
    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override {
        task->append_accesses(p_reads, p_writes);
    }
    virtual void bind_resources(const RendResViewVK& p_resources) override {
        task->bind_resources(p_resources);
    }
    /**
     * @brief Records the stage unless the construction is completed.
     */
    virtual void record(VkCommandBuffer p_buffer, double p_delta) override;

private:
    Unique<IRenderTaskVK> task;
    Ref<bool> completed;
    bool finish;
    std::string completion_message;
};

} // namespace WhiteBirdEngine

#endif
