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
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "core/parser/parser_json.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/shader_metadata_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

bool is_descriptor_binding(const ShaderMetadataBinding& p_binding) {
    return p_binding.kind == "descriptorTableSlot";
}

bool entry_point_uses_binding(const ShaderMetadataEntryPoint& p_entry_point, const ShaderMetadataBinding& p_binding) {
    if (p_entry_point.bindings.empty()) {
        return true;
    }
    return std::ranges::any_of(p_entry_point.bindings, [&p_binding](const ShaderMetadataBinding& p_entry_point_binding) {
        return is_descriptor_binding(p_entry_point_binding) && p_entry_point_binding.space == p_binding.space &&
               p_entry_point_binding.index == p_binding.index;
    });
}

uint32_t get_entry_point_descriptor_count(const ShaderMetadataEntryPoint& p_entry_point, const ShaderMetadataBinding& p_binding) {
    for (const auto& entry_point_binding : p_entry_point.bindings) {
        if (is_descriptor_binding(entry_point_binding) && entry_point_binding.space == p_binding.space &&
            entry_point_binding.index == p_binding.index) {
            return static_cast<uint32_t>(std::max(entry_point_binding.count, 1));
        }
    }
    return static_cast<uint32_t>(std::max(p_binding.count, 1));
}

VkDescriptorType descriptor_type_from_parameter(const ShaderMetadataParameter& p_parameter) {
    if (p_parameter.type.kind == "constantBuffer") {
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    if (p_parameter.type.kind == "resource" && p_parameter.type.scalar_type.starts_with("texture")) {
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }
    if (p_parameter.type.kind == "samplerState") {
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    throw std::runtime_error(std::format(
        "Failed to translate shader descriptor '{}': unsupported parameter type '{}'.", p_parameter.name, p_parameter.type.kind));
}

VkDescriptorType descriptor_type_from_field(std::string_view p_field_path, const ShaderMetadataTypeField& p_field) {
    if (p_field.type.kind == "constantBuffer" || (p_field.type.kind == "array" && p_field.type.element_scalar_type == "constantBuffer")) {
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    if ((p_field.type.kind == "resource" && p_field.type.scalar_type.starts_with("texture")) ||
        (p_field.type.kind == "array" && p_field.type.element_scalar_type.starts_with("texture"))) {
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }
    if (p_field.type.kind == "samplerState" || (p_field.type.kind == "array" && p_field.type.element_scalar_type == "samplerState")) {
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    throw std::runtime_error(std::format("Failed to translate shader descriptor '{}': unsupported parameter-block field type '{}'.",
        p_field_path,
        p_field.type.kind));
}

VkDeviceSize reflected_binding_size(const ShaderMetadataBinding& p_binding) {
    return p_binding.size <= 0 ? 0 : static_cast<VkDeviceSize>(p_binding.size);
}

VkDeviceSize reflected_binding_offset(const ShaderMetadataBinding& p_binding) {
    return p_binding.offset <= 0 ? 0 : static_cast<VkDeviceSize>(p_binding.offset);
}

std::string shader_parameter_type_name(const ShaderMetadataParameter& p_parameter) {
    if (!p_parameter.type.element_type.name.empty()) {
        return p_parameter.type.element_type.name;
    }
    return p_parameter.type.name;
}

std::string descriptor_field_path(const ShaderMetadataParameter& p_parameter, const ShaderMetadataTypeField& p_field) {
    return std::format("{}.{}", p_parameter.name, p_field.name);
}

std::string descriptor_field_path(std::string_view p_parameter_name, const ShaderMetadataTypeField& p_field) {
    return std::format("{}.{}", p_parameter_name, p_field.name);
}

std::vector<ShaderDescriptorFieldBindingVK> collect_descriptor_field_bindings(const ShaderMetadataParameter& p_parameter) {
    std::vector<ShaderDescriptorFieldBindingVK> result;
    result.reserve(p_parameter.type.fields.size());
    for (const auto& field : p_parameter.type.fields) {
        if (field.binding.kind.empty()) {
            continue;
        }
        VkDeviceSize field_size = reflected_binding_size(field.binding);
        if (field_size == 0) {
            continue;
        }
        result.push_back(ShaderDescriptorFieldBindingVK{
            .field_path = descriptor_field_path(p_parameter, field),
            .offset = reflected_binding_offset(field.binding),
            .size = field_size,
        });
    }
    return result;
}

uint32_t descriptor_count_from_field(const ShaderMetadataTypeField& p_field) {
    if (p_field.type.kind == "array") {
        return static_cast<uint32_t>(std::max(p_field.type.element_count, 0));
    }
    return static_cast<uint32_t>(std::max(p_field.binding.count, 1));
}

ShaderDescriptorBindingVK make_parameter_block_field_binding(
    const ShaderMetadataParameter& p_parameter, const ShaderMetadataTypeField& p_field, VkShaderStageFlagBits p_stage_flag) {
    if (!is_descriptor_binding(p_field.binding)) {
        throw std::runtime_error(std::format(
            "Failed to translate parameter block '{}': field '{}' is not descriptor-backed.", p_parameter.name, p_field.name));
    }
    std::string field_path = descriptor_field_path(p_parameter.name, p_field);
    VkDescriptorType descriptor_type = descriptor_type_from_field(field_path, p_field);
    bool variable_descriptor_count = p_field.type.kind == "array" && p_field.type.element_count == 0;
    if (variable_descriptor_count && descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
        throw std::runtime_error(std::format(
            "Failed to translate runtime descriptor array '{}': only sampled-image arrays are supported.", field_path));
    }
    ShaderDescriptorBindingVK descriptor_binding;
    descriptor_binding.parameter_name = field_path;
    descriptor_binding.type_name = p_field.type.name;
    descriptor_binding.buffer_size = static_cast<VkDeviceSize>(std::max(p_field.buffer_size, 0));
    descriptor_binding.set_index = static_cast<uint32_t>(std::max(p_parameter.binding.index, 0));
    descriptor_binding.layout_binding = VkDescriptorSetLayoutBinding{
        .binding = static_cast<uint32_t>(std::max(p_field.binding.index, 0)),
        .descriptorType = descriptor_type,
        .descriptorCount = descriptor_count_from_field(p_field),
        .stageFlags = p_stage_flag,
    };
    descriptor_binding.variable_descriptor_count = variable_descriptor_count;
    return descriptor_binding;
}

VkDeviceSize descriptor_buffer_size(
    const ShaderMetadataParameter& p_parameter, const std::vector<ShaderDescriptorFieldBindingVK>& p_fields) {
    VkDeviceSize buffer_size = reflected_binding_size(p_parameter.binding);
    for (const auto& field : p_fields) {
        buffer_size = std::max(buffer_size, field.offset + field.size);
    }
    return buffer_size;
}

void merge_descriptor_binding(std::vector<ShaderDescriptorBindingVK>& p_bindings, const ShaderDescriptorBindingVK& p_binding) {
    auto existing = std::ranges::find_if(p_bindings, [&p_binding](const ShaderDescriptorBindingVK& p_existing) {
        return p_existing.set_index == p_binding.set_index && p_existing.layout_binding.binding == p_binding.layout_binding.binding;
    });
    if (existing == p_bindings.end()) {
        p_bindings.push_back(p_binding);
        return;
    }
    // Vertex and fragment stages can reference the same set/binding. In that
    // case the layout binding is shared and the stage flags are merged.
    if (existing->layout_binding.descriptorType != p_binding.layout_binding.descriptorType ||
        existing->layout_binding.descriptorCount != p_binding.layout_binding.descriptorCount ||
        existing->variable_descriptor_count != p_binding.variable_descriptor_count) {
        throw std::runtime_error(std::format("Conflicting shader descriptor declarations for set {}, binding {}.",
            p_binding.set_index,
            p_binding.layout_binding.binding));
    }
    existing->layout_binding.stageFlags |= p_binding.layout_binding.stageFlags;
    if (existing->type_name.empty()) {
        existing->type_name = p_binding.type_name;
    }
    existing->buffer_size = std::max(existing->buffer_size, p_binding.buffer_size);
    for (const auto& field : p_binding.fields) {
        auto existing_field = std::ranges::find_if(existing->fields, [&field](const ShaderDescriptorFieldBindingVK& p_existing) {
            return p_existing.field_path == field.field_path;
        });
        if (existing_field == existing->fields.end()) {
            existing->fields.push_back(field);
            continue;
        }
        if (existing_field->offset != field.offset || existing_field->size != field.size) {
            throw std::runtime_error(std::format("Conflicting shader descriptor field declarations for '{}'.", field.field_path));
        }
    }
}

void append_stage_descriptor_bindings(
    std::vector<ShaderDescriptorBindingVK>& p_bindings, const ShaderMetadata* p_metadata, std::string_view p_entry_name, std::string_view p_stage) {
    if (p_metadata == nullptr) {
        return;
    }
    std::string entry_name = p_entry_name.data() == nullptr ? std::string() : std::string(p_entry_name);
    auto stage_bindings = collect_shader_descriptor_bindings(*p_metadata, p_stage, entry_name);
    for (const auto& binding : stage_bindings) {
        merge_descriptor_binding(p_bindings, binding);
    }
}

} // namespace

ShaderMetadata load_shader_metadata_vk(RefRes<RUShader> p_shader) {
    if (p_shader == nullptr || p_shader->metadata_path == nullptr) {
        throw std::runtime_error("Failed to load shader metadata: metadata path is missing.");
    }
    ParserJSON metadata_parser;
    metadata_parser.parse(get_resource_path(p_shader->metadata_path.get()));
    ShaderMetadata metadata;
    metadata.deserialize(metadata_parser.get_data());
    return metadata;
}

const ShaderMetadataEntryPoint& resolve_shader_stage_entry_point(
    const ShaderMetadata& p_metadata, std::string_view p_stage, std::string_view p_entry_name) {
    if (!p_entry_name.empty()) {
        return find_shader_entry_point(p_metadata, p_stage, p_entry_name);
    }
    const ShaderMetadataEntryPoint* matched_entry_point = nullptr;
    for (const auto& entry_point : p_metadata.entry_points) {
        if (entry_point.stage != p_stage) {
            continue;
        }
        if (matched_entry_point != nullptr) {
            throw std::runtime_error(std::format("Failed to resolve shader entry point for stage '{}': multiple reflected entries exist, so the pipeline must specify one.",
                p_stage));
        }
        matched_entry_point = &entry_point;
    }
    if (matched_entry_point == nullptr) {
        throw std::runtime_error(std::format("Failed to find shader entry point for stage '{}'.", p_stage));
    }
    return *matched_entry_point;
}

const ShaderMetadataEntryPoint& find_shader_entry_point(
    const ShaderMetadata& p_metadata, std::string_view p_stage, std::string_view p_entry_name) {
    auto entry_point =
        std::ranges::find_if(p_metadata.entry_points, [&p_stage, &p_entry_name](const ShaderMetadataEntryPoint& p_entry_point) {
            return p_entry_point.stage == p_stage && p_entry_point.name == p_entry_name;
        });
    if (entry_point == p_metadata.entry_points.end()) {
        throw std::runtime_error(std::format("Failed to find shader entry point '{}:{}'.", p_stage, p_entry_name));
    }
    return *entry_point;
}

VkShaderStageFlagBits shader_stage_flag_from_metadata_stage(std::string_view p_stage) {
    if (p_stage == "vertex" || p_stage == "vert") {
        return VK_SHADER_STAGE_VERTEX_BIT;
    }
    if (p_stage == "fragment" || p_stage == "frag") {
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if (p_stage == "geometry" || p_stage == "geom") {
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    }
    if (p_stage == "tesscontrol" || p_stage == "tesc") {
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    }
    if (p_stage == "tesseval" || p_stage == "tese") {
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }
    throw std::runtime_error(std::format("Unsupported shader stage '{}'.", p_stage));
}

std::vector<ShaderDescriptorBindingVK> collect_shader_descriptor_bindings(
    const ShaderMetadata& p_metadata, std::string_view p_stage, std::string_view p_entry_name) {
    const auto& entry_point = resolve_shader_stage_entry_point(p_metadata, p_stage, p_entry_name);
    VkShaderStageFlagBits stage_flag = shader_stage_flag_from_metadata_stage(entry_point.stage);
    std::vector<ShaderDescriptorBindingVK> result;
    for (const auto& parameter : p_metadata.parameters) {
        if (parameter.type.kind == "parameterBlock") {
            if (parameter.binding.kind != "subElementRegisterSpace") {
                throw std::runtime_error(std::format(
                    "Failed to translate parameter block '{}': expected subElementRegisterSpace binding.", parameter.name));
            }
            for (const auto& field : parameter.type.fields) {
                merge_descriptor_binding(result, make_parameter_block_field_binding(parameter, field, stage_flag));
            }
            continue;
        }
        if (!is_descriptor_binding(parameter.binding) || !entry_point_uses_binding(entry_point, parameter.binding)) {
            continue;
        }
        ShaderDescriptorBindingVK descriptor_binding;
        descriptor_binding.parameter_name = parameter.name;
        descriptor_binding.type_name = shader_parameter_type_name(parameter);
        descriptor_binding.fields = collect_descriptor_field_bindings(parameter);
        descriptor_binding.buffer_size = descriptor_buffer_size(parameter, descriptor_binding.fields);
        descriptor_binding.set_index = static_cast<uint32_t>(parameter.binding.space);
        descriptor_binding.layout_binding = VkDescriptorSetLayoutBinding{
            .binding = static_cast<uint32_t>(parameter.binding.index),
            .descriptorType = descriptor_type_from_parameter(parameter),
            .descriptorCount = get_entry_point_descriptor_count(entry_point, parameter.binding),
            .stageFlags = stage_flag,
        };
        merge_descriptor_binding(result, descriptor_binding);
    }
    return result;
}

std::vector<ShaderDescriptorBindingVK> collect_graphics_pipeline_descriptor_bindings(const ShaderMetadata& p_vertex_shader_metadata,
    std::string_view p_vertex_stage_entry,
    const ShaderMetadata* p_geometry_shader_metadata,
    std::string_view p_geometry_stage_entry,
    const ShaderMetadata& p_fragment_shader_metadata,
    std::string_view p_fragment_stage_entry) {
    std::vector<ShaderDescriptorBindingVK> result;
    append_stage_descriptor_bindings(result, &p_vertex_shader_metadata, p_vertex_stage_entry, "vertex");
    append_stage_descriptor_bindings(result, p_geometry_shader_metadata, p_geometry_stage_entry, "geometry");
    append_stage_descriptor_bindings(result, &p_fragment_shader_metadata, p_fragment_stage_entry, "fragment");
    return result;
}

std::vector<ShaderDescriptorSetLayoutVK> build_graphics_pipeline_descriptor_set_layout_bindings(
    const std::vector<ShaderDescriptorBindingVK>& p_bindings) {
    if (p_bindings.empty()) {
        return {};
    }
    uint32_t max_set_index = 0;
    for (const auto& binding : p_bindings) {
        max_set_index = std::max(max_set_index, binding.set_index);
    }
    std::vector<ShaderDescriptorSetLayoutVK> result(max_set_index + 1);
    for (uint32_t i = 0; i < result.size(); ++i) {
        result[i].set_index = i;
    }
    for (const auto& binding : p_bindings) {
        result[binding.set_index].bindings.push_back(binding.layout_binding);
    }
    for (auto& set_layout : result) {
        std::ranges::sort(set_layout.bindings, [](const VkDescriptorSetLayoutBinding& p_lhs, const VkDescriptorSetLayoutBinding& p_rhs) {
            return p_lhs.binding < p_rhs.binding;
        });
        set_layout.binding_flags.reserve(set_layout.bindings.size());
        for (const auto& layout_binding : set_layout.bindings) {
            auto descriptor_binding =
                std::ranges::find_if(p_bindings, [&set_layout, &layout_binding](const ShaderDescriptorBindingVK& p_binding) {
                    return p_binding.set_index == set_layout.set_index && p_binding.layout_binding.binding == layout_binding.binding;
                });
            VkDescriptorBindingFlags binding_flags = 0;
            if (descriptor_binding != p_bindings.end()) {
                if (descriptor_binding->variable_descriptor_count) {
                    binding_flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                }
            }
            set_layout.binding_flags.push_back(binding_flags);
        }
    }
    return result;
}

void set_variable_descriptor_count_upper_bounds(std::vector<ShaderDescriptorSetLayoutVK>& p_set_layouts, uint32_t p_max_descriptor_count) {
    uint64_t fixed_descriptor_count = 0;
    uint32_t variable_binding_count = 0;
    for (const auto& set_layout : p_set_layouts) {
        if (set_layout.bindings.size() != set_layout.binding_flags.size()) {
            throw std::runtime_error("Failed to assign variable descriptor bounds: binding flags do not match bindings.");
        }
        for (size_t i = 0; i < set_layout.bindings.size(); ++i) {
            if ((set_layout.binding_flags[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0) {
                ++variable_binding_count;
            } else {
                fixed_descriptor_count += set_layout.bindings[i].descriptorCount;
            }
        }
    }
    if (variable_binding_count == 0) {
        return;
    }
    if (fixed_descriptor_count >= p_max_descriptor_count || p_max_descriptor_count - fixed_descriptor_count < variable_binding_count) {
        throw std::runtime_error("Failed to assign variable descriptor bounds: the fixed descriptors exhaust the descriptor budget.");
    }
    uint32_t variable_descriptor_upper_bound =
        static_cast<uint32_t>((p_max_descriptor_count - fixed_descriptor_count) / variable_binding_count);
    for (auto& set_layout : p_set_layouts) {
        for (size_t i = 0; i < set_layout.bindings.size(); ++i) {
            if ((set_layout.binding_flags[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0) {
                set_layout.bindings[i].descriptorCount = variable_descriptor_upper_bound;
            }
        }
    }
}

} // namespace WhiteBirdEngine
