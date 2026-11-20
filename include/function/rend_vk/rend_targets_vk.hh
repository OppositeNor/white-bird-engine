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
#ifndef WBE_FILE_REND_TARGETS_VK_HH
#define WBE_FILE_REND_TARGETS_VK_HH

#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/rend_object_container_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_graphics_pipeline_vk.hh"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class RendResViewVK;

/**
 * @class RendTargetsVK
 * @brief The dynamic-rendering attachments of one graphics task.
 *
 * Graphics tasks render into the task-local resources `rend_target_colors[<index>]` and `rend_target_depth`. The task
 * descriptor maps them to graph resources with RendTaskDescVK::alias(). This object reports their accesses, binds their
 * containers, and begins rendering into them.
 *
 * The object is copyable and movable; copies share the bound containers. It is confined to the rendering thread.
 */
class RendTargetsVK {
public:
    /**
     * @class Config
     * @brief Clear values and load operations of the render targets.
     */
    struct Config {
        /** Clear values for the color targets followed by the depth target. Empty clears every target to zero. */
        std::span<const VkClearValue> clear_values;
        /** Load operation of every color target. Loading also reads the target. */
        VkAttachmentLoadOp color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        /** Load operation of the depth target. Loading also reads the target. */
        VkAttachmentLoadOp depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    };

    /** Task-local identifier of the depth render target. */
    static constexpr std::string_view DEPTH_ID = "rend_target_depth";

    /**
     * @brief Get the task-local identifier of a color render target.
     * @param p_index Color attachment index in pipeline order.
     * @return `rend_target_colors[<p_index>]`.
     */
    static std::string color_id(uint32_t p_index);

    RendTargetsVK() = delete;

    /**
     * @brief Configure render targets with an explicit layout.
     * @param p_alloc Allocator for owned storage; it must outlive this object.
     * @param p_color_count Number of color targets.
     * @param p_has_depth Whether a depth target is used.
     * @param p_config Clear values and load operations.
     * @throws std::runtime_error If clear values are supplied but fewer than the targets.
     */
    RendTargetsVK(RendAlloc* p_alloc, uint32_t p_color_count, bool p_has_depth, const Config& p_config);

    /**
     * @brief Configure render targets with the attachment layout of a graphics pipeline.
     * @param p_alloc Allocator for owned storage; it must outlive this object.
     * @param p_pipeline Pipeline whose color formats and depth format select the targets.
     * @param p_config Clear values and load operations.
     * @throws std::runtime_error If clear values are supplied but fewer than the targets.
     */
    RendTargetsVK(RendAlloc* p_alloc, const ROGraphicsPipelineVK& p_pipeline, const Config& p_config);

    ~RendTargetsVK() = default;
    RendTargetsVK(const RendTargetsVK&) = default;
    RendTargetsVK(RendTargetsVK&&) noexcept = default;
    RendTargetsVK& operator=(const RendTargetsVK&) = default;
    RendTargetsVK& operator=(RendTargetsVK&&) noexcept = default;

    /**
     * @brief Check whether a graphics pipeline renders into the same attachment layout.
     * @param p_pipeline The pipeline.
     * @return True when the color count and depth usage match.
     */
    bool matches(const ROGraphicsPipelineVK& p_pipeline) const;

    /**
     * @brief Append the render-target accesses: writes of every target, and reads of loaded targets.
     * @param p_reads Receives read accesses.
     * @param p_writes Receives write accesses.
     */
    void append_accesses(Vector<ResAccessVK, RendAlloc>& p_reads, Vector<ResAccessVK, RendAlloc>& p_writes) const;

    /**
     * @brief Resolve and retain the containers of every render target.
     * @param p_resources The task's resource view.
     * @throws std::runtime_error If a render target is not mapped to a declared graph resource.
     */
    void bind_resources(const RendResViewVK& p_resources);

    /**
     * @brief Begin dynamic rendering into the render targets of the current frame slot.
     * @param p_rend_data The base render data.
     * @param p_command_buffer The command buffer.
     * @param p_rend_extent The render area.
     */
    void begin_rendering(const RendContextVK* p_rend_data, VkCommandBuffer p_command_buffer, VkExtent2D p_rend_extent) const;

private:
    uint32_t color_count = 0;
    bool has_depth = false;
    Vector<VkClearValue, RendAlloc> clear_values;
    VkAttachmentLoadOp color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentLoadOp depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Vector<Ref<RendObjectContainerVK>, RendAlloc> color_containers;
    Ref<RendObjectContainerVK> depth_container;
};

} // namespace WhiteBirdEngine

#endif
