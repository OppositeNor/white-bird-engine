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
#include "function/render_vk/ro_image_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {


ROImageVK::ROImageVK(const RendererVK::BaseRenderDataVK* p_render_data, const Config& p_config)
    : render_data(p_render_data) {
    init(p_config);
}

ROImageVK::~ROImageVK() {
    vkDestroyImage(render_data->logic_device, image, render_data->allocation_callbacks);
}

void ROImageVK::init(const Config& p_config) {
    // TODO
}

}
