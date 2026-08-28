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
#ifndef WBE_FILE_RT_GENERATE_MIPMAPS_VK_HH
#define WBE_FILE_RT_GENERATE_MIPMAPS_VK_HH

#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <cstdint>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTGenerateMipmapsVK
 * @brief Generates a color image's mip chain with filtered Vulkan blits.
 *
 * The underlying image must contain every requested mip level and support
 * transfer-source, transfer-destination, and linear blit operations. Instances
 * are neither copyable nor movable.
 */
struct RTGenerateMipmapsVK final : public RenderTaskVK {
    RTGenerateMipmapsVK() = delete;
    virtual ~RTGenerateMipmapsVK() override = default;
    RTGenerateMipmapsVK(const RTGenerateMipmapsVK&) = delete;
    RTGenerateMipmapsVK(RTGenerateMipmapsVK&&) noexcept = delete;
    RTGenerateMipmapsVK& operator=(const RTGenerateMipmapsVK&) = delete;
    RTGenerateMipmapsVK& operator=(RTGenerateMipmapsVK&&) noexcept = delete;

    /**
     * @brief Construct a mip-generation task.
     *
     * @param p_render_data Base Vulkan render data.
     * @param p_image_view_id Render-graph resource id of the image view.
     * @param p_base_extent Extent of mip level zero.
     * @param p_mipmap_level_count Total number of mip levels, including level zero.
     * @param p_format Image format used for capability validation.
     * @throws std::runtime_error If the format cannot perform filtered blits.
     */
    RTGenerateMipmapsVK(const BaseRenderDataVK* p_render_data,
        std::string_view p_image_view_id,
        VkExtent2D p_base_extent,
        uint32_t p_mipmap_level_count,
        VkFormat p_format);

    /** @copydoc RenderTaskVK::queue_family */
    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    /** @copydoc RenderTaskVK::read_resources */
    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;

    /** @copydoc RenderTaskVK::write_resources */
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;

    /** @copydoc RenderTaskVK::record */
    virtual void record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta) override;

private:
    const BaseRenderDataVK* render_data = nullptr;
    std::string image_view_id;
    VkExtent2D base_extent{};
    uint32_t mipmap_level_count = 1;
};

} // namespace WhiteBirdEngine

#endif
