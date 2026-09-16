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
#ifndef WBE_FILE_RT_RUN_COMPUTE_PIPELINE_2D_VK_HH
#define WBE_FILE_RT_RUN_COMPUTE_PIPELINE_2D_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunComputePipeline2DVK
 * @brief Render task that dispatches two synchronized compute passes in one command buffer.
 * Shader resource reads and writes are declared fluently on RendTaskDescVK.
 * This is useful for 2D operations such as SAT and convolutions.
 */
struct RTRunComputePipeline2DVK final : public RenderTaskVK {
    RTRunComputePipeline2DVK() = delete;
    virtual ~RTRunComputePipeline2DVK() override = default;
    RTRunComputePipeline2DVK(const RTRunComputePipeline2DVK&) = delete;
    RTRunComputePipeline2DVK(RTRunComputePipeline2DVK&&) noexcept = delete;
    RTRunComputePipeline2DVK& operator=(const RTRunComputePipeline2DVK&) = delete;
    RTRunComputePipeline2DVK& operator=(RTRunComputePipeline2DVK&&) noexcept = delete;

    /**
     * @brief Constructor.
     *
     * @param p_render_data Base render data.
     * @param p_pipeline_x First compute pipeline retained by the task.
     * @param p_pipeline_y Second compute pipeline retained by the task.
     * @param p_dispatch_group_count_x Workgroup counts for the first dispatch.
     * @param p_dispatch_group_count_y Workgroup counts for the second dispatch.
     * @param p_queue_family Queue family used to submit the compute dispatch.
     */
    RTRunComputePipeline2DVK(const BaseRenderDataVK* p_render_data,
        Ref<ROComputePipelineVK> p_pipeline_x,
        Ref<ROComputePipelineVK> p_pipeline_y,
        VkExtent3D p_dispatch_group_count_x,
        VkExtent3D p_dispatch_group_count_y,
        QueueFamilyVK p_queue_family = QueueFamilyVK::COMPUTE);

    virtual QueueFamilyVK queue_family() const override {
        return queue_family_id;
    }

    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;

    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    QueueFamilyVK queue_family_id = QueueFamilyVK::COMPUTE;
    Ref<ROComputePipelineVK> pipeline_x;
    Ref<ROComputePipelineVK> pipeline_y;
    VkExtent3D dispatch_group_count_x{.width = 1, .height = 1, .depth = 1};
    VkExtent3D dispatch_group_count_y{.width = 1, .height = 1, .depth = 1};
};

} // namespace WhiteBirdEngine

#endif
