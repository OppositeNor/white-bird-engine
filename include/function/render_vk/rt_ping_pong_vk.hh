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
#ifndef WBE_FILE_RT_PING_PONG_VK_HH
#define WBE_FILE_RT_PING_PONG_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTPingPongVK
 * @brief Render task that alternates between two compute pipelines for a fixed number of iterations.
 *
 * A false state selects the ping pipeline and a true state selects the pong pipeline. Shader resource reads and writes are
 * declared fluently on RendTaskDescVK.
 */
struct RTPingPongVK final : public IRenderTaskVK {
    RTPingPongVK() = delete;
    virtual ~RTPingPongVK() override = default;
    RTPingPongVK(const RTPingPongVK&) = delete;
    RTPingPongVK(RTPingPongVK&&) noexcept = delete;
    RTPingPongVK& operator=(const RTPingPongVK&) = delete;
    RTPingPongVK& operator=(RTPingPongVK&&) noexcept = delete;

    /**
     * @brief Constructor.
     *
     * @param p_render_data Base render data.
     * @param p_ping_pipeline Compute pipeline selected by a false state.
     * @param p_pong_pipeline Compute pipeline selected by a true state.
     * @param p_initial_state Selection used by the first iteration; false selects ping and true selects pong.
     * @param p_iterations Number of alternating pipeline dispatches recorded by the task.
     * @param p_dispatch_group_count Workgroup counts passed to vkCmdDispatch.
     * @param p_queue_family Queue family used to submit the compute dispatch.
     */
    RTPingPongVK(const BaseRenderDataVK* p_render_data,
        Ref<ROComputePipelineVK> p_ping_pipeline,
        Ref<ROComputePipelineVK> p_pong_pipeline,
        bool p_initial_state,
        uint32_t p_iterations,
        VkExtent3D p_dispatch_group_count,
        QueueFamilyVK p_queue_family = QueueFamilyVK::COMPUTE);

    /**
     * @brief Construct or configure invocation-local bindings by copying the supplied mappings.
     * @details Mapping references are borrowed only for this call and are not retained.
     */
    RTPingPongVK(const BaseRenderDataVK* p_render_data,
        Ref<ROComputePipelineVK> p_ping_pipeline,
        Ref<ROComputePipelineVK> p_pong_pipeline,
        bool p_initial_state,
        uint32_t p_iterations,
        VkExtent3D p_dispatch_group_count,
        QueueFamilyVK p_queue_family,
        const PipelineResourceMapVK& p_ping_resource_map,
        const PipelineResourceMapVK& p_pong_resource_map);

    /**
     * @brief Configure one preparatory compute dispatch before each ping or pong dispatch.
     *
     * This supports algorithms that must clear the destination state before each alternating iteration. Both prefix
     * pipelines must be provided and this method must be called before render-graph resource binding.
     *
     * @param p_ping_prefix_pipeline Pipeline recorded before every ping dispatch.
     * @param p_pong_prefix_pipeline Pipeline recorded before every pong dispatch.
     * @param p_dispatch_group_count Workgroup counts used by both prefix pipelines.
     */
    void set_iteration_prefixes(Ref<ROComputePipelineVK> p_ping_prefix_pipeline,
        Ref<ROComputePipelineVK> p_pong_prefix_pipeline,
        VkExtent3D p_dispatch_group_count);

    /**
     * @brief Construct or configure invocation-local bindings by copying the supplied mappings.
     * @details Mapping references are borrowed only for this call and are not retained.
     */
    void set_iteration_prefixes(Ref<ROComputePipelineVK> p_ping_prefix_pipeline,
        Ref<ROComputePipelineVK> p_pong_prefix_pipeline,
        VkExtent3D p_dispatch_group_count,
        const PipelineResourceMapVK& p_ping_resource_map,
        const PipelineResourceMapVK& p_pong_resource_map);

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
    Ref<ROComputePipelineVK> ping_pipeline;
    Ref<ROComputePipelineVK> pong_pipeline;
    Unique<PipelineDescriptorBindingsVK> ping_descriptor_bindings;
    Unique<PipelineDescriptorBindingsVK> pong_descriptor_bindings;
    Ref<ROComputePipelineVK> ping_prefix_pipeline;
    Ref<ROComputePipelineVK> pong_prefix_pipeline;
    Unique<PipelineDescriptorBindingsVK> ping_prefix_descriptor_bindings;
    Unique<PipelineDescriptorBindingsVK> pong_prefix_descriptor_bindings;
    VkExtent3D dispatch_group_count{.width = 1, .height = 1, .depth = 1};
    VkExtent3D prefix_dispatch_group_count{.width = 1, .height = 1, .depth = 1};
    bool resources_bound = false;
    bool initial_state = false;
    uint32_t iterations = 0;
};

} // namespace WhiteBirdEngine

#endif
