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
#include "function/rend_vk/rend_targets_vk.hh"
#include "core/alloc/i_alloc.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_graphics_pipeline_vk.hh"
#include "function/rend_vk/ro_image_view_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

uint32_t pipeline_color_count(const ROGraphicsPipelineVK& p_pipeline) {
    return static_cast<uint32_t>(p_pipeline.get_config().attachment_config.color_formats.size());
}

bool pipeline_has_depth(const ROGraphicsPipelineVK& p_pipeline) {
    const auto& depth_format = p_pipeline.get_config().attachment_config.depth_formats;
    return depth_format.has_value() && *depth_format != VK_FORMAT_UNDEFINED;
}

} // namespace

std::string RendTargetsVK::color_id(uint32_t p_index) {
    return std::format("rend_target_colors[{}]", p_index);
}

RendTargetsVK::RendTargetsVK(RendAlloc* p_alloc, uint32_t p_color_count, bool p_has_depth, const Config& p_config)
    : color_count(p_color_count), has_depth(p_has_depth), clear_values(p_alloc), color_load_op(p_config.color_load_op),
      depth_load_op(p_config.depth_load_op), color_containers(p_alloc) {
    const size_t target_count = static_cast<size_t>(color_count) + (has_depth ? 1 : 0);
    if (!p_config.clear_values.empty() && p_config.clear_values.size() < target_count) {
        throw std::runtime_error(std::format(
            "RendTargetsVK: {} clear values were supplied for {} render targets.", p_config.clear_values.size(), target_count));
    }
    clear_values.assign(target_count, VkClearValue{});
    for (size_t i = 0; i < target_count && i < p_config.clear_values.size(); ++i) {
        clear_values[i] = p_config.clear_values[i];
    }
}

RendTargetsVK::RendTargetsVK(RendAlloc* p_alloc, const ROGraphicsPipelineVK& p_pipeline, const Config& p_config)
    : RendTargetsVK(p_alloc, pipeline_color_count(p_pipeline), pipeline_has_depth(p_pipeline), p_config) {
}

bool RendTargetsVK::matches(const ROGraphicsPipelineVK& p_pipeline) const {
    return pipeline_color_count(p_pipeline) == color_count && pipeline_has_depth(p_pipeline) == has_depth;
}

void RendTargetsVK::append_accesses(Vector<ResAccessVK, RendAlloc>& p_reads, Vector<ResAccessVK, RendAlloc>& p_writes) const {
    for (uint32_t i = 0; i < color_count; ++i) {
        if (color_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            p_reads.push_back({.res_id = color_id(i), .usage = RendResUsageVK::COLOR_ATTACHMENT});
        }
        p_writes.push_back({.res_id = color_id(i), .usage = RendResUsageVK::COLOR_ATTACHMENT});
    }
    if (has_depth) {
        if (depth_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            p_reads.push_back({.res_id = std::string(DEPTH_ID), .usage = RendResUsageVK::DEPTH_ATTACHMENT});
        }
        p_writes.push_back({.res_id = std::string(DEPTH_ID), .usage = RendResUsageVK::DEPTH_ATTACHMENT});
    }
}

void RendTargetsVK::bind_resources(const RendResViewVK& p_resources) {
    color_containers.clear();
    for (uint32_t i = 0; i < color_count; ++i) {
        color_containers.push_back(p_resources.get(color_id(i)).container);
    }
    depth_container = has_depth ? p_resources.get(DEPTH_ID).container : MEM_NULL;
}

void RendTargetsVK::begin_rendering(
    const RendContextVK* p_rend_data, VkCommandBuffer p_command_buffer, VkExtent2D p_rend_extent) const {
    WBE_DEBUG_ASSERT(color_containers.size() == color_count);
    Vector<VkRenderingAttachmentInfo, RendAlloc> color_attachments(p_rend_data->alloc);
    color_attachments.reserve(color_count);
    for (uint32_t i = 0; i < color_count; ++i) {
        auto image_view =
            WBE_REQUIRES_VALID(color_containers[i]->get(p_rend_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>());
        color_attachments.push_back(ROImageViewVK::AttachmentBuilder::color(image_view->get_image_view())
                .set_load_op(color_load_op)
                .set_clear_value(clear_values[i])
                .build());
    }
    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = {.offset = {.x = 0, .y = 0}, .extent = p_rend_extent};
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = color_count;
    rendering_info.pColorAttachments = color_attachments.data();
    VkRenderingAttachmentInfo depth_attachment{};
    if (has_depth) {
        auto image_view =
            WBE_REQUIRES_VALID(depth_container->get(p_rend_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>());
        depth_attachment = ROImageViewVK::AttachmentBuilder::depth(image_view->get_image_view())
                               .set_load_op(depth_load_op)
                               .set_store_op(VK_ATTACHMENT_STORE_OP_STORE)
                               .set_clear_value(clear_values[color_count])
                               .build();
        rendering_info.pDepthAttachment = &depth_attachment;
    }
    vkCmdBeginRendering(p_command_buffer, &rendering_info);
}

} // namespace WhiteBirdEngine
