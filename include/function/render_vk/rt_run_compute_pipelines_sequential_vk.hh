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
#ifndef WBE_FILE_RT_RUN_COMPUTE_PIPELINES_SEQUENTIAL_VK_HH
#define WBE_FILE_RT_RUN_COMPUTE_PIPELINES_SEQUENTIAL_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include <optional>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunComputePipelinesSequentialVK
 * @brief Render task that dispatches an ordered sequence of synchronized compute passes in one command buffer.
 * Shader resource reads and writes are declared fluently on RendTaskDescVK.
 */
struct RTRunComputePipelinesSequentialVK final : public IRenderTaskVK {
    /**
     * @class Entry
     * @brief One retained compute pipeline plus its workgroup counts.
     */
    struct Entry {
        /** Compute pipeline retained by the dispatch and task. */
        Ref<ROComputePipelineVK> pipeline = MEM_NULL;
        /** Workgroup count dispatched in each dimension. */
        VkExtent3D group_count{.width = 1, .height = 1, .depth = 1};
        /** Optional invocation-local mapping, copied with this entry. */
        std::optional<PipelineResourceMapVK> resource_map;
    };

    /**
     * @class Dispatch
     * @brief Allocator-owned ordered compute-pass entries.
     *
     * Dispatches are copyable and movable. Their allocator must outlive every copy retaining entries.
     */
    struct Dispatch {
        explicit Dispatch(RenderAllocator* p_allocator);
        ~Dispatch() = default;
        Dispatch(const Dispatch&) = default;
        Dispatch(Dispatch&&) noexcept = default;
        Dispatch& operator=(const Dispatch&) = default;
        Dispatch& operator=(Dispatch&&) noexcept = default;

        /**
         * @brief Append one compute pass.
         * @param p_entry Entry transferred into the dispatch.
         * @return This dispatch.
         */
        Dispatch& add_entry(Entry p_entry);

        /**
         * @brief Append one compute pass from its pipeline and workgroup count.
         * @param p_pipeline Compute pipeline retained by the dispatch.
         * @param p_group_count Workgroup count dispatched in each dimension.
         * @return This dispatch.
         */
        Dispatch& add_entry(Ref<ROComputePipelineVK> p_pipeline, VkExtent3D p_group_count);

        /** Ordered compute passes. */
        Vector<Entry, RenderAllocator> entries;
    };

    RTRunComputePipelinesSequentialVK() = delete;
    virtual ~RTRunComputePipelinesSequentialVK() override = default;
    RTRunComputePipelinesSequentialVK(const RTRunComputePipelinesSequentialVK&) = delete;
    RTRunComputePipelinesSequentialVK(RTRunComputePipelinesSequentialVK&&) noexcept = delete;
    RTRunComputePipelinesSequentialVK& operator=(const RTRunComputePipelinesSequentialVK&) = delete;
    RTRunComputePipelinesSequentialVK& operator=(RTRunComputePipelinesSequentialVK&&) noexcept = delete;

    /**
     * @brief Constructor.
     *
     * @param p_render_data Base render data.
     * @param p_dispatch Ordered compute entries retained by the task.
     * @param p_queue_family Queue family used to submit the compute dispatches.
     */
    RTRunComputePipelinesSequentialVK(
        const BaseRenderDataVK* p_render_data, const Dispatch& p_dispatch, QueueFamilyVK p_queue_family = QueueFamilyVK::COMPUTE);

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
    Vector<Entry, RenderAllocator> entries;
    Vector<Unique<PipelineDescriptorBindingsVK>, RenderAllocator> descriptor_bindings;
};

} // namespace WhiteBirdEngine

#endif
