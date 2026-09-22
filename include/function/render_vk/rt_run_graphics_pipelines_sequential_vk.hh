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
#ifndef WBE_FILE_RT_RUN_GRAPHICS_PIPELINES_SEQUENTIAL_VK_HH
#define WBE_FILE_RT_RUN_GRAPHICS_PIPELINES_SEQUENTIAL_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <optional>
#include <string>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTRunGraphicsPipelinesSequentialVK
 * @brief Records ordered, synchronized graphics passes in one command buffer.
 *
 * Attachment and draw resources are intrinsic task resources. Shader resources are declared on RendTaskDescVK.
 */
struct RTRunGraphicsPipelinesSequentialVK final : public IRenderTaskVK {
    /**
     * @class Entry
     * @brief Configuration for one graphics pass in the ordered sequence.
     *
     * Entries are copyable and movable. Their allocator must outlive every copy that retains their containers.
     */
    struct Entry {
        /**
         * @brief Construct an empty pass configuration using the supplied allocator.
         * @param p_allocator Allocator used by owned attachment identifiers and clear values.
         */
        explicit Entry(RenderAllocator* p_allocator);
        ~Entry() = default;
        Entry(const Entry&) = default;
        Entry(Entry&&) noexcept = default;
        Entry& operator=(const Entry&) = default;
        Entry& operator=(Entry&&) noexcept = default;

        /** Graphics pipeline retained by the task. */
        Ref<ROGraphicsPipelineVK> pipeline = MEM_NULL;
        /** Clear values for color attachments followed by the optional depth attachment. */
        Vector<VkClearValue, RenderAllocator> clear_values;
        /** Render area for this pass. */
        VkExtent2D render_extent{};
        /** Color attachment graph-resource identifiers in pipeline order. */
        Vector<std::string, RenderAllocator> color_attachment_ids;
        /** Optional depth attachment graph-resource identifier. */
        std::optional<std::string> depth_attachment_id;
        /** Optional vertex-buffer graph-resource identifier. */
        std::optional<std::string> vertex_buffer_id;
        /** Optional index-buffer graph-resource identifier. */
        std::optional<std::string> index_buffer_id;
        /** Optional indexed subregion; valid only when vertex and index buffers are configured. */
        std::optional<IndexedDrawRegionVK> indexed_draw_region;
        /** Load operation used by every color attachment. */
        VkAttachmentLoadOp color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        /** Load operation used by the optional depth attachment. */
        VkAttachmentLoadOp depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        /** Dynamic vertex-input state for indexed draws. */
        VertexInputDescriptionVK vertex_input;
        /** Byte offset bound for the optional vertex buffer. */
        VkDeviceSize vertex_buffer_offset = 0;
        /** Vertex count used when no vertex and index buffers are configured. */
        uint32_t draw_vertex_count = 3;
    };

    /**
     * @class Dispatch
     * @brief Allocator-owned ordered graphics-pass entries.
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
         * @brief Append one graphics pass.
         * @param p_entry Entry transferred into the dispatch.
         * @return This dispatch.
         */
        Dispatch& add_entry(Entry p_entry);

        /** Ordered graphics passes. */
        Vector<Entry, RenderAllocator> entries;
    };

    RTRunGraphicsPipelinesSequentialVK() = delete;
    virtual ~RTRunGraphicsPipelinesSequentialVK() override = default;
    RTRunGraphicsPipelinesSequentialVK(const RTRunGraphicsPipelinesSequentialVK&) = delete;
    RTRunGraphicsPipelinesSequentialVK(RTRunGraphicsPipelinesSequentialVK&&) noexcept = delete;
    RTRunGraphicsPipelinesSequentialVK& operator=(const RTRunGraphicsPipelinesSequentialVK&) = delete;
    RTRunGraphicsPipelinesSequentialVK& operator=(RTRunGraphicsPipelinesSequentialVK&&) noexcept = delete;

    /**
     * @brief Construct an ordered graphics-pass task.
     * @param p_render_data Renderer state used to retain and record the passes.
     * @param p_dispatch Ordered pass configurations copied into task-owned storage.
     * @throws std::runtime_error When no pass is provided or a pass configuration is invalid.
     */
    RTRunGraphicsPipelinesSequentialVK(const BaseRenderDataVK* p_render_data, const Dispatch& p_dispatch);

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void bind_resources(const RendResViewVK& p_resources) override;
    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;
    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    struct BoundEntry {
        BoundEntry(RenderAllocator* p_allocator, const Entry& p_entry);
        ~BoundEntry() = default;
        BoundEntry(const BoundEntry&) = default;
        BoundEntry(BoundEntry&&) noexcept = default;
        BoundEntry& operator=(const BoundEntry&) = default;
        BoundEntry& operator=(BoundEntry&&) noexcept = default;

        Entry entry;
        Vector<Ref<RenderObjectContainerVK>, RenderAllocator> color_attachment_containers;
        Ref<RenderObjectContainerVK> depth_attachment_container;
        Ref<RenderObjectContainerVK> vertex_buffer_container;
        Ref<RenderObjectContainerVK> index_buffer_container;
    };

    const BaseRenderDataVK* render_data = nullptr;
    Vector<BoundEntry, RenderAllocator> entries;
};

} // namespace WhiteBirdEngine

#endif
