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
#include "function/render_vk/debug_utils_vk.hh"
#include "core/core_utils.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TypeID ROShaderVK::get_type_uuid() const {
    return TypeUUID<ROShaderVK>::UUID;
}

ROShaderVK::Builder& ROShaderVK::Builder::set_code(std::span<const uint32_t> p_code) {
    code = std::vector<uint32_t>(p_code.begin(), p_code.end());
    code_size = p_code.size();
    return *this;
}

ROShaderVK::Builder& ROShaderVK::Builder::set_code(std::string_view p_resource_id) {
    auto resource = ResourceManager::get_singleton()->get_resource<RUShader>(p_resource_id);
    return set_code(resource);
}

ROShaderVK::Builder& ROShaderVK::Builder::set_code(RefRes<RUShader> p_resource) {
    if (p_resource == nullptr || p_resource->data == nullptr) {
        throw std::runtime_error("Failed to set shader code: shader resource is missing binary data.");
    }
    return set_code(std::span(p_resource->data.get(), p_resource->size));
}

VkShaderModule ROShaderVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    WBE_REQUIRED(code);
    create_info.pCode = (*code).data();
    create_info.codeSize = WBE_REQUIRED(code_size) * sizeof(decltype(*(*code).data()));

    VkShaderModule result = VK_NULL_HANDLE;
    if (vkCreateShaderModule(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
    return result;
}

ROShaderVK::ROShaderVK(const BaseRenderDataVK* p_base_render_data, const Builder& p_builder)
    : IRenderObjectVK(p_base_render_data) {
    init(p_builder);
}

ROShaderVK::ROShaderVK(const BaseRenderDataVK* p_base_render_data, std::string_view p_resource_id)
    : IRenderObjectVK(p_base_render_data) {
    Builder builder;
    builder.set_code(p_resource_id);
    init(builder);
}

ROShaderVK::ROShaderVK(const BaseRenderDataVK* p_base_render_data, RefRes<RUShader> p_resource)
    : IRenderObjectVK(p_base_render_data) {
    Builder builder;
    builder.set_code(p_resource);
    init(builder);
}

VkShaderModule ROShaderVK::get_shader_module() const {
    return shader_module;
}

ROShaderVK::~ROShaderVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call(
        [render_data = render_data, shader_module = shader_module]() {
            vkDestroyShaderModule(render_data->logic_device, shader_module, render_data->allocation_callbacks);
        },
        render_data->frames_in_flight);
}

void ROShaderVK::init(const Builder& p_builder) {
    shader_module = p_builder.create_vk_object(render_data);
}

void ROShaderVK::set_debug_name(std::string_view p_name) {
    set_debug_object_name_vk(render_data, VK_OBJECT_TYPE_SHADER_MODULE, shader_module, p_name);
}

} // namespace WhiteBirdEngine
