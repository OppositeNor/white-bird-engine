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
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunComputePipelineVK
 * @brief Render task that binds a compute pipeline and dispatches workgroups.
 *
 * Resource binding contract:
 *   reads:  [compute_pipeline, additional read resources...]
 *   writes: [additional write resources...]
 * The additional access lists let callers describe buffer/image dependencies
 * for render-graph barriers while this task records the compute dispatch.
 */
struct RTRunComputePipelineVK final : public RenderTaskVK {
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
     * @param p_pipeline_id Resource id of the compute pipeline.
     * @param p_dispatch_group_count Workgroup counts passed to vkCmdDispatch.
     * @param p_descriptor_sets Descriptor sets to bind in order.
     * @param p_read_accesses Additional resources read by the compute shader.
     * @param p_write_accesses Additional resources written by the compute shader.
     * @param p_queue_family Queue family used to submit the compute dispatch.
     */
    RTRunComputePipelineVK(const BaseRenderDataVK* p_render_data,
        std::string_view p_pipeline_id,
        VkExtent3D p_dispatch_group_count,
        std::span<const Ref<RODescriptorSetVK>> p_descriptor_sets = {},
        std::span<const ResAccessVK> p_read_accesses = {},
        std::span<const ResAccessVK> p_write_accesses = {},
        QueueFamilyVK p_queue_family = QueueFamilyVK::COMPUTE);

    virtual QueueFamilyVK queue_family() const override {
        return queue_family_id;
    }

    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;

    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;

    virtual void record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    QueueFamilyVK queue_family_id = QueueFamilyVK::COMPUTE;
    std::string pipeline_id;
    VkExtent3D dispatch_group_count{.width = 1, .height = 1, .depth = 1};
    Vector<Ref<RODescriptorSetVK>, RenderAllocator> descriptor_sets;
    Vector<VkDescriptorSet, RenderAllocator> descriptor_sets_instances;
    Vector<ResAccessVK, RenderAllocator> additional_read_accesses;
    Vector<ResAccessVK, RenderAllocator> additional_write_accesses;
};

} // namespace WhiteBirdEngine

#endif
