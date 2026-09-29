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
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTClearAttachmentsVK
 * @brief Generic graphics task that performs a clear/load rendering pass without issuing a draw.
 *
 * The task renders into `render_target_colors[<index>]` and `render_target_depth`; map them to graph resources with
 * RendTaskDescVK::alias().
 */
class RTClearAttachmentsVK final : public IRenderTaskVK {
public:
    RTClearAttachmentsVK() = delete;
    ~RTClearAttachmentsVK() override = default;
    RTClearAttachmentsVK(const RTClearAttachmentsVK&) = delete;
    RTClearAttachmentsVK(RTClearAttachmentsVK&&) noexcept = delete;
    RTClearAttachmentsVK& operator=(const RTClearAttachmentsVK&) = delete;
    RTClearAttachmentsVK& operator=(RTClearAttachmentsVK&&) noexcept = delete;

    /**
     * @brief Construct a no-draw dynamic-rendering pass.
     * @param p_render_data Base render data.
     * @param p_render_extent Render area.
     * @param p_color_count Number of color render targets.
     * @param p_has_depth Whether the depth render target is used.
     * @param p_config Clear values and load operations.
     * @throws std::runtime_error If the render extent is empty or clear values are too few.
     */
    RTClearAttachmentsVK(const BaseRenderDataVK* p_render_data,
        VkExtent2D p_render_extent,
        uint32_t p_color_count,
        bool p_has_depth,
        const RenderTargetsVK::Config& p_config);

    QueueFamilyVK queue_family() const override;
    void bind_resources(const RendResViewVK& p_resources) override;
    void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override;
    void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    const BaseRenderDataVK* render_data;
    VkExtent2D render_extent{};
    RenderTargetsVK render_targets;
};

} // namespace WhiteBirdEngine

#endif
