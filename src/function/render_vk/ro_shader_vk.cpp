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

#include "function/render_vk/ro_shader_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROShaderVK::ROShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, const Config& p_config)
: render_data(p_base_render_data) {
    init(p_config);
}

VkShaderModule ROShaderVK::get_shader_module() const {
    return shader_module;
}

ROShaderVK::~ROShaderVK() {
    vkDestroyShaderModule(render_data->logic_device, shader_module, render_data->allocation_callbacks);
}

void ROShaderVK::init(const Config& p_config) {
    shader_module = create_shader_module(p_config);
}

VkShaderModule ROShaderVK::create_shader_module(const Config& p_config) {
    VkShaderModuleCreateInfo create_info{};
    VkShaderModule result;
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = p_config.code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(p_config.code.data());
    if (vkCreateShaderModule(render_data->logic_device, &create_info, render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
    return result;
}

}
