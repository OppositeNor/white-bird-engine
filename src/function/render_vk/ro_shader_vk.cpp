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
#include "function/render_vk/render_vk_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROShaderVK::Builder& ROShaderVK::Builder::set_code(const std::vector<char>& p_code) {
    code = p_code;
    return *this;
}

VkShaderModule ROShaderVK::Builder::build(const BaseRenderDataVK* p_render_data) const {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    const auto& shader_code = WBE_REQUIRED(code);
    create_info.codeSize = shader_code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
    
    VkShaderModule result;
    if (vkCreateShaderModule(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
    return result;
}

ROShaderVK::ROShaderVK(const BaseRenderDataVK* p_base_render_data, const Builder& p_builder)
    : render_data(p_base_render_data) {
    init(p_builder);
}

VkShaderModule ROShaderVK::get_shader_module() const {
    return shader_module;
}

ROShaderVK::~ROShaderVK() {
    vkDestroyShaderModule(render_data->logic_device, shader_module, render_data->allocation_callbacks);
}

void ROShaderVK::init(const Builder& p_builder) {
    shader_module = p_builder.build(render_data);
}

}
