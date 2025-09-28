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
#ifndef __WBE_RO_IMAGE_LAYOUT_VK_HH__
#define __WBE_RO_IMAGE_LAYOUT_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class ROImageLayoutVK final : public RenderObject {
public:
    struct Config {
        // TODO
    };
    ROImageLayoutVK(const RendererVK::BaseRenderDataVK* p_render_data, const Config& p_config);
    virtual ~ROImageLayoutVK() override;

    virtual std::string get_ro_type() const override {
        return "image_layout_vk";
    }

    /**
     * @brief Get the image layout instance.
     *
     * @return The image layout instance.
     */
    VkImageLayout get_image_layout() const {
        return image_layout;
    }

private:
    const RendererVK::BaseRenderDataVK* render_data;
    VkImageLayout image_layout;

    void init(const RendererVK::BaseRenderDataVK* p_render_data, const Config& p_config);
};

}

#endif
