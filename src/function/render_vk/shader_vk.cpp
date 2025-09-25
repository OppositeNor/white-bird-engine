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
#include "function/render_vk/shader_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

VkShaderModule ShaderVK::create_shader_module(const RendererVK::BaseRenderDataVK* p_base_render_data, const char* p_code_data, size_t p_code_size) {
    VkShaderModuleCreateInfo create_info{};
    VkShaderModule result;
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = p_code_size;
    create_info.pCode = reinterpret_cast<const uint32_t*>(p_code_data);
    if (vkCreateShaderModule(p_base_render_data->logic_device, &create_info, p_base_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
    return result;
}


}
