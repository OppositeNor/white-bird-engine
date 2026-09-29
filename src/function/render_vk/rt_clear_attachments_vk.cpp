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
#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
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

RTClearAttachmentsVK::RTClearAttachmentsVK(
    const BaseRenderDataVK* p_render_data, VkExtent2D p_render_extent, uint32_t p_color_count, bool p_has_depth, const RenderTargetsVK::Config& p_config)
    : render_data(p_render_data), render_extent(p_render_extent),
      render_targets(p_render_data->allocator, p_color_count, p_has_depth, p_config) {
    if (render_extent.width == 0 || render_extent.height == 0) {
        throw std::runtime_error("RTClearAttachmentsVK: render extent must be non-zero.");
    }
}

QueueFamilyVK RTClearAttachmentsVK::queue_family() const {
    return QueueFamilyVK::GRAPHICS;
}

void RTClearAttachmentsVK::bind_resources(const RendResViewVK& p_resources) {
    render_targets.bind_resources(p_resources);
}

void RTClearAttachmentsVK::append_accesses(
    Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const {
    render_targets.append_accesses(p_reads, p_writes);
}

void RTClearAttachmentsVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    render_targets.begin_rendering(render_data, p_command_buffer, render_extent);
    vkCmdEndRendering(p_command_buffer);
}

} // namespace WhiteBirdEngine
