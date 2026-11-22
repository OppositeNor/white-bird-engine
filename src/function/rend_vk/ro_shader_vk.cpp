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

#include "function/rend_vk/ro_shader_vk.hh"
#include "function/rend_vk/debug_utils_vk.hh"
#include "core/core_utils.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/render/shader_variant_resolver.hh"
#include "function/rend_vk/i_rend_object_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

Ref<ROShaderVK> ShaderDescVK::create_ref(const RendContextVK* p_rend_context) const {
    WBE_DEBUG_ASSERT(p_rend_context != nullptr);
    return p_rend_context->create_shader(*this);
}

Unique<ROShaderVK> ShaderDescVK::create_unique(const RendContextVK* p_rend_context) const {
    WBE_DEBUG_ASSERT(p_rend_context != nullptr);
    return make_unique<ROShaderVK>(p_rend_context->alloc, p_rend_context, *this);
}

TypeID ROShaderVK::get_type_uuid() const {
    return TypeUUID<ROShaderVK>::UUID;
}

ShaderDescVK& ShaderDescVK::set_code(std::span<const uint32_t> p_code) {
    default_variant_index = 0;
    variants = std::vector<Variant>{
        Variant{.name = "default", .code = std::vector<uint32_t>(p_code.begin(), p_code.end())}};
    return *this;
}

ShaderDescVK& ShaderDescVK::set_code(std::string_view p_resource_id) {
    auto resource = ResourceManager::get_singleton()->get_resource<RUShader>(p_resource_id);
    return set_code(resource);
}

ShaderDescVK& ShaderDescVK::set_code(RefRes<RUShader> p_resource) {
    if (p_resource == nullptr) {
        throw std::runtime_error("Failed to set shader code: shader resource is missing.");
    }
    std::vector<Variant> resource_variants;
    for (const RUShaderVariant& variant : p_resource->get_variants()) {
        if (variant.data == nullptr) {
            throw std::runtime_error("Failed to set shader code: shader resource variant is missing binary data.");
        }
        resource_variants.push_back(Variant{.name = variant.name.get(), .code = std::vector<uint32_t>(variant.data.get(), variant.data.get() + variant.size)});
    }
    default_variant_index = p_resource->default_variant_index;
    variants = std::move(resource_variants);
    return *this;
}

VkShaderModule ShaderDescVK::create_vk_object(const RendContextVK* p_rend_data, const Variant& p_variant) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pCode = p_variant.code.data();
    create_info.codeSize = p_variant.code.size() * sizeof(uint32_t);

    VkShaderModule result = VK_NULL_HANDLE;
    if (vkCreateShaderModule(p_rend_data->logic_device, &create_info, p_rend_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
    return result;
}

ROShaderVK::ROShaderVK(const RendContextVK* p_rend_context, const Builder& p_builder)
    : IRendObjectVK(p_rend_context) {
    init(p_builder);
}

ROShaderVK::ROShaderVK(const RendContextVK* p_rend_context, std::string_view p_resource_id)
    : IRendObjectVK(p_rend_context) {
    Builder builder;
    builder.set_code(p_resource_id);
    init(builder);
}

ROShaderVK::ROShaderVK(const RendContextVK* p_rend_context, RefRes<RUShader> p_resource)
    : IRendObjectVK(p_rend_context) {
    Builder builder;
    builder.set_code(p_resource);
    init(builder);
}

VkShaderModule ROShaderVK::get_shader_module() const {
    return get_variant({});
}

VkShaderModule ROShaderVK::get_variant(std::string_view p_name) const {
    return ShaderVariantResolver()
        .find(std::span<const VariantModule>(variant_modules), default_variant_index, p_name,
            [](const VariantModule& p_variant) {
                return std::string_view(p_variant.name);
            },
            "shader object")
        .module;
}

ROShaderVK::~ROShaderVK() {
    // Deallocate resource after frames in flight.
    rend_data->register_delayed_call(
        [rend_data = rend_data, variant_modules = std::move(variant_modules)]() {
            for (const VariantModule& variant_module : variant_modules) {
                vkDestroyShaderModule(rend_data->logic_device, variant_module.module, rend_data->allocation_callbacks);
            }
        },
        rend_data->frames_in_flight);
}

void ROShaderVK::init(const Builder& p_builder) {
    const std::vector<ShaderDescVK::Variant>& variants = WBE_REQUIRED(p_builder.variants);
    if (variants.empty()) {
        throw std::runtime_error("Failed to create shader: the description has no variants.");
    }
    default_variant_index = p_builder.default_variant_index;
    if (default_variant_index >= variants.size()) {
        throw std::runtime_error("Failed to create shader: the default variant index is out of range.");
    }
    variant_modules.reserve(variants.size());
    for (const ShaderDescVK::Variant& variant : variants) {
        if (!variant_modules.empty() && variant_modules.back().name >= variant.name) {
            throw std::runtime_error("Failed to create shader: variants must be sorted by unique name.");
        }
        variant_modules.push_back(VariantModule{.name = variant.name, .module = ShaderDescVK::create_vk_object(rend_data, variant)});
    }
}

void ROShaderVK::apply_debug_name(std::string_view p_name) {
    for (const VariantModule& variant_module : variant_modules) {
        set_debug_object_name_vk(rend_data, VK_OBJECT_TYPE_SHADER_MODULE, variant_module.module, p_name);
    }
}

} // namespace WhiteBirdEngine
