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
#ifndef __WBE_RO_IMAGE_VK_HH__
#define __WBE_RO_IMAGE_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROImageVK
 * @brief Image.
 *
 */
class ROImageVK final : public RenderObject {
public:
    // Image configurations.
    struct Config {
        // Image type.
        VkImageType image_type;
        // Image format.
        VkFormat format;
        // Image extent.
        VkExtent3D extent;
        // Mipmap level.
        uint32_t mipmap_level = 1;
        // Number of layers.
        uint32_t layers_count = 1;
    };

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_config Configuration.
     */
    ROImageVK(const RendererVK::BaseRenderDataVK* p_render_data, const Config& p_config);
    virtual ~ROImageVK() override;

    virtual std::string get_ro_type() const override {
        return "image_vk";
    }

    /**
     * @brief Get image instance.
     *
     * @return The image instance.
     */
    VkImage get_image() const {
        return image;
    }

private:
    VkImage image;
    const RendererVK::BaseRenderDataVK* render_data;

    void init(const Config& p_config);
};

}

#endif
