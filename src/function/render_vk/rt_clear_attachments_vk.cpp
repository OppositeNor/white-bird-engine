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
#include "function/render_vk/rt_clear_attachments_vk.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTClearAttachmentsVK::RTClearAttachmentsVK(const BaseRenderDataVK* p_render_data,
    std::span<const VkClearValue> p_clear_values,
    VkExtent2D p_render_extent,
    std::span<const std::string_view> p_color_attachment_ids,
    std::optional<std::string_view> p_depth_attachment_id,
    VkAttachmentLoadOp p_color_attachment_load_op,
    VkAttachmentLoadOp p_depth_attachment_load_op)
    : render_data(p_render_data), clear_values(p_clear_values.begin(), p_clear_values.end(), p_render_data->allocator),
      render_extent(p_render_extent),
      color_attachment_ids(p_color_attachment_ids.begin(), p_color_attachment_ids.end(), p_render_data->allocator),
      depth_attachment_id(p_depth_attachment_id.has_value() ? std::optional<std::string>(*p_depth_attachment_id) : std::nullopt),
      color_attachment_load_op(p_color_attachment_load_op), depth_attachment_load_op(p_depth_attachment_load_op),
      color_attachment_containers(p_render_data->allocator) {
    const size_t required_clear_value_count = color_attachment_ids.size() + (depth_attachment_id.has_value() ? 1U : 0U);
    if (render_extent.width == 0 || render_extent.height == 0) {
        throw std::runtime_error("RTClearAttachmentsVK: render extent must be non-zero.");
    }
    if (clear_values.size() < required_clear_value_count) {
        throw std::runtime_error("RTClearAttachmentsVK: insufficient clear values.");
    }
}

QueueFamilyVK RTClearAttachmentsVK::queue_family() const {
    return QueueFamilyVK::GRAPHICS;
}

void RTClearAttachmentsVK::bind_resources(const RendResViewVK& p_resources) {
    color_attachment_containers.clear();
    for (const std::string& id : color_attachment_ids) {
        color_attachment_containers.push_back(p_resources.get(id).container);
    }
    if (depth_attachment_id.has_value()) {
        depth_attachment_container = p_resources.get(*depth_attachment_id).container;
    }
}

Vector<ResAccessVK, RenderAllocator> RTClearAttachmentsVK::read_resources() const {
    Vector<ResAccessVK, RenderAllocator> result(render_data->allocator);
    if (color_attachment_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
        for (const std::string& id : color_attachment_ids) {
            result.push_back({.res_id = id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
        }
    }
    if (depth_attachment_id.has_value() && depth_attachment_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
        result.push_back({.res_id = *depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
    }
    return result;
}

Vector<ResAccessVK, RenderAllocator> RTClearAttachmentsVK::write_resources() const {
    Vector<ResAccessVK, RenderAllocator> result(render_data->allocator);
    for (const std::string& id : color_attachment_ids) {
        result.push_back({.res_id = id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
    }
    if (depth_attachment_id.has_value()) {
        result.push_back({.res_id = *depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
    }
    return result;
}

void RTClearAttachmentsVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    Vector<VkRenderingAttachmentInfo, RenderAllocator> color_attachments(render_data->allocator);
    for (size_t index = 0; index < color_attachment_ids.size(); ++index) {
        Ref<ROImageViewVK> view = WBE_REQUIRES_VALID(
            color_attachment_containers[index]->get(render_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>());
        color_attachments.push_back(ROImageViewVK::AttachmentBuilder::color(view->get_image_view())
                .set_load_op(color_attachment_load_op)
                .set_clear_value(clear_values[index])
                .build());
    }
    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {.offset = {.x = 0, .y = 0}, .extent = render_extent},
        .layerCount = 1,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
        .pColorAttachments = color_attachments.data(),
    };
    VkRenderingAttachmentInfo depth_attachment{};
    if (depth_attachment_id.has_value()) {
        Ref<ROImageViewVK> view = WBE_REQUIRES_VALID(
            depth_attachment_container->get(render_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>());
        depth_attachment = ROImageViewVK::AttachmentBuilder::depth(view->get_image_view())
                               .set_load_op(depth_attachment_load_op)
                               .set_store_op(VK_ATTACHMENT_STORE_OP_STORE)
                               .set_clear_value(clear_values[color_attachments.size()])
                               .build();
        rendering_info.pDepthAttachment = &depth_attachment;
    }
    vkCmdBeginRendering(p_command_buffer, &rendering_info);
    vkCmdEndRendering(p_command_buffer);
}

} // namespace WhiteBirdEngine
