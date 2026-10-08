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
#ifndef WBE_FILE_RT_RUN_COMPUTE_PIPELINE_INDIRECT_VK_HH
#define WBE_FILE_RT_RUN_COMPUTE_PIPELINE_INDIRECT_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunComputePipelineIndirectVK
 * @brief Render task that binds a compute pipeline and dispatches the workgroup counts a buffer holds.
 * @details The task reports its reflected shader accesses and an indirect read of the indirect buffer; map reflected
 * resource ids to graph resources with RendTaskDescVK::alias(). Neither copyable nor movable.
 */
struct RTRunComputePipelineIndirectVK final : public IRenderTaskVK {
    RTRunComputePipelineIndirectVK() = delete;
    virtual ~RTRunComputePipelineIndirectVK() override = default;
    RTRunComputePipelineIndirectVK(const RTRunComputePipelineIndirectVK&) = delete;
    RTRunComputePipelineIndirectVK(RTRunComputePipelineIndirectVK&&) noexcept = delete;
    RTRunComputePipelineIndirectVK& operator=(const RTRunComputePipelineIndirectVK&) = delete;
    RTRunComputePipelineIndirectVK& operator=(RTRunComputePipelineIndirectVK&&) noexcept = delete;

    /**
     * @brief Constructor.
     *
     * @param p_render_data Base render data.
     * @param p_pipeline Compute pipeline retained by the task.
     * @param p_indirect_buffer_id Graph resource id of the buffer holding the VkDispatchIndirectCommand; the buffer must be
     * created with indirect usage.
     * @param p_indirect_offset Byte offset of the VkDispatchIndirectCommand in the indirect buffer; must be a multiple of 4.
     * @param p_queue_family Queue used to submit the compute dispatch: GRAPHICS, COMPUTE_1, or COMPUTE_2.
     * @throws std::runtime_error If p_pipeline is null, p_indirect_buffer_id is empty, or p_indirect_offset is not a multiple
     * of 4.
     */
    RTRunComputePipelineIndirectVK(const BaseRenderDataVK* p_render_data,
        Ref<ROComputePipelineVK> p_pipeline,
        std::string_view p_indirect_buffer_id,
        VkDeviceSize p_indirect_offset,
        QueueFamilyVK p_queue_family = QueueFamilyVK::GRAPHICS);

    virtual QueueFamilyVK queue_family() const override {
        return queue_family_id;
    }

    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override;

    virtual void bind_resources(const RendResViewVK& p_resources) override;

    /**
     * @brief Record the indirect dispatch.
     * @throws std::runtime_error If the indirect buffer is too small to hold a VkDispatchIndirectCommand at the offset.
     */
    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    QueueFamilyVK queue_family_id = QueueFamilyVK::GRAPHICS;
    Ref<ROComputePipelineVK> pipeline;
    Unique<PipelineDescriptorBindingsVK> descriptor_bindings;
    std::string indirect_buffer_id;
    VkDeviceSize indirect_offset = 0;
    Ref<RenderObjectContainerVK> indirect_buffer_container;
};

} // namespace WhiteBirdEngine

#endif
