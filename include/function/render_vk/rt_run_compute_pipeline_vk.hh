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
#ifndef WBE_FILE_RT_RUN_COMPUTE_PIPELINE_VK_HH
#define WBE_FILE_RT_RUN_COMPUTE_PIPELINE_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
#include "function/render_vk/pipeline_resource_map_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunComputePipelineVK
 * @brief Render task that binds a compute pipeline and dispatches workgroups.
 * Shader resource reads and writes are declared fluently on RendTaskDescVK.
 */
struct RTRunComputePipelineVK final : public IRenderTaskVK {
    RTRunComputePipelineVK() = delete;
    virtual ~RTRunComputePipelineVK() override = default;
    RTRunComputePipelineVK(const RTRunComputePipelineVK&) = delete;
    RTRunComputePipelineVK(RTRunComputePipelineVK&&) noexcept = delete;
    RTRunComputePipelineVK& operator=(const RTRunComputePipelineVK&) = delete;
    RTRunComputePipelineVK& operator=(RTRunComputePipelineVK&&) noexcept = delete;

    /**
     * @brief Constructor.
     *
     * @param p_render_data Base render data.
     * @param p_pipeline Compute pipeline retained by the task.
     * @param p_dispatch_group_count Workgroup counts passed to vkCmdDispatch.
     * @param p_queue_family Queue family used to submit the compute dispatch.
     */
    RTRunComputePipelineVK(const BaseRenderDataVK* p_render_data,
        Ref<ROComputePipelineVK> p_pipeline,
        VkExtent3D p_dispatch_group_count,
        QueueFamilyVK p_queue_family = QueueFamilyVK::COMPUTE);

    /**
     * @brief Construct or configure invocation-local bindings by copying the supplied mappings.
     * @details Mapping references are borrowed only for this call and are not retained.
     */
    RTRunComputePipelineVK(const BaseRenderDataVK* p_render_data,
        Ref<ROComputePipelineVK> p_pipeline,
        VkExtent3D p_dispatch_group_count,
        QueueFamilyVK p_queue_family,
        const PipelineResourceMapVK& p_resource_map);

    virtual QueueFamilyVK queue_family() const override {
        return queue_family_id;
    }

    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;

    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;

    virtual void bind_resources(const RendResViewVK& p_resources) override;

    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    QueueFamilyVK queue_family_id = QueueFamilyVK::COMPUTE;
    Ref<ROComputePipelineVK> pipeline;
    Unique<PipelineDescriptorBindingsVK> descriptor_bindings;
    VkExtent3D dispatch_group_count{.width = 1, .height = 1, .depth = 1};
};

} // namespace WhiteBirdEngine

#endif
