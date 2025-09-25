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
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ShaderVK::ShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, ResourceChunkID p_resource_chunk, const std::string& p_shader_res_id)
    : base_render_data(p_base_render_data) {
    ResourceManager* resource_manager = ResourceManager::get_singleton();
    RUBinary* resource = resource_manager->get_resource<RUBinary>(p_resource_chunk, p_shader_res_id);
    const char* code_data = resource_manager->get_resource_chunk(p_resource_chunk)->get_data<const char>(resource->data);
    initialize_shader(code_data, resource->size);
}

ShaderVK::ShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, const Path& p_shader_file_path)
    : base_render_data(p_base_render_data) {
    auto binary_data = load_binary_file(static_cast<std::string>(p_shader_file_path).c_str());
    initialize_shader(binary_data.data(), binary_data.size());
}

ShaderVK::ShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, const char* p_code_data, size_t p_data_size)
    : base_render_data(p_base_render_data) {
    initialize_shader(p_code_data, p_data_size);
}

ShaderVK::~ShaderVK() {
    vkDestroyShaderModule(base_render_data->logic_device, shader_module, base_render_data->allocation_callbacks);
}

void ShaderVK::initialize_shader(const char* p_code_data, size_t p_code_size) {

}

void ShaderVK::setup_shader_module(const char* p_code_data, size_t p_code_size) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = p_code_size;
    create_info.pCode = reinterpret_cast<const uint32_t*>(p_code_data);
    if (vkCreateShaderModule(base_render_data->logic_device, &create_info, base_render_data->allocation_callbacks, &shader_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
}

}
