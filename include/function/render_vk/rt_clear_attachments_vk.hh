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
#ifndef WBE_FILE_RT_CLEAR_ATTACHMENTS_VK_HH
#define WBE_FILE_RT_CLEAR_ATTACHMENTS_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTClearAttachmentsVK
 * @brief Generic graphics task that performs a clear/load rendering pass without issuing a draw.
 */
class RTClearAttachmentsVK final : public IRenderTaskVK {
public:
    RTClearAttachmentsVK() = delete;
    ~RTClearAttachmentsVK() override = default;
    RTClearAttachmentsVK(const RTClearAttachmentsVK&) = delete;
    RTClearAttachmentsVK(RTClearAttachmentsVK&&) noexcept = delete;
    RTClearAttachmentsVK& operator=(const RTClearAttachmentsVK&) = delete;
    RTClearAttachmentsVK& operator=(RTClearAttachmentsVK&&) noexcept = delete;

    /** @brief Construct a no-draw dynamic-rendering pass. */
    RTClearAttachmentsVK(const BaseRenderDataVK* p_render_data,
        std::span<const VkClearValue> p_clear_values,
        VkExtent2D p_render_extent,
        std::span<const std::string_view> p_color_attachment_ids,
        std::optional<std::string_view> p_depth_attachment_id,
        VkAttachmentLoadOp p_color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentLoadOp p_depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR);

    QueueFamilyVK queue_family() const override;
    void bind_resources(const RendResViewVK& p_resources) override;
    Vector<ResAccessVK, RenderAllocator> read_resources() const override;
    Vector<ResAccessVK, RenderAllocator> write_resources() const override;
    void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data;
    Vector<VkClearValue, RenderAllocator> clear_values;
    VkExtent2D render_extent{};
    Vector<std::string, RenderAllocator> color_attachment_ids;
    std::optional<std::string> depth_attachment_id;
    VkAttachmentLoadOp color_attachment_load_op;
    VkAttachmentLoadOp depth_attachment_load_op;
    Vector<Ref<RenderObjectContainerVK>, RenderAllocator> color_attachment_containers;
    Ref<RenderObjectContainerVK> depth_attachment_container;
};

} // namespace WhiteBirdEngine

#endif
