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
namespace WhiteBirdEngine {

/**
 * @class ROImageVK
 * @brief Image.
 *
 */
class ROImageVK final : public RenderObject {
public:
    struct Config {
        // TODO;
    };

    ROImageVK(const RendererVK::BaseRenderDataVK* p_render_data, const Config& p_config);
    virtual ~ROImageVK() override;

    virtual std::string get_ro_type() const override {
        return "image_vk";
    }
};

}

#endif
