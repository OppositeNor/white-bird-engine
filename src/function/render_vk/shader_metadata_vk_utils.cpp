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
#include "function/render/render_database.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_query.hh"
#include "function/render/vertex_input_contract.hh"
#include "function/render/vertex_input_contract_resolver.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

bool entry_point_uses_push_data(const ShaderMetadataEntryPoint& p_entry_point, const ShaderMetadataBinding& p_binding) {
    if (p_entry_point.bindings.empty()) {
        return true;
    }
    return std::ranges::any_of(p_entry_point.bindings, [&p_binding](const ShaderMetadataBinding& p_entry_point_binding) {
        return p_entry_point_binding.kind == "pushConstantBuffer" && p_entry_point_binding.index == p_binding.index;
    });
}

ShaderDescriptorHeapTypeVK descriptor_heap_type_from_name(std::string_view p_name) {
    if (p_name == "uniform_buffer") {
        return ShaderDescriptorHeapTypeVK::UniformBuffer;
    }
    if (p_name == "storage_buffer") {
        return ShaderDescriptorHeapTypeVK::StorageBuffer;
    }
    if (p_name == "sampled_image") {
        return ShaderDescriptorHeapTypeVK::SampledImage;
    }
    if (p_name == "storage_image") {
        return ShaderDescriptorHeapTypeVK::StorageImage;
    }
    if (p_name == "sampler") {
        return ShaderDescriptorHeapTypeVK::Sampler;
    }
    throw std::runtime_error(std::format("Failed to resolve WBE_DESCRIPTOR: unsupported descriptor type '{}'.", p_name));
}

const ShaderMetadataUserAttribute* find_descriptor_attribute(const ShaderMetadataTypeField& p_field) {
    const ShaderMetadataUserAttribute* result = nullptr;
    for (const ShaderMetadataUserAttribute& attribute : p_field.user_attributes) {
        if (attribute.name != "WBE_DESCRIPTOR") {
            continue;
        }
        if (result != nullptr) {
            throw std::runtime_error(std::format("Failed to resolve WBE_DESCRIPTOR: field '{}' repeats the annotation.", p_field.name));
        }
        result = &attribute;
    }
    return result;
}

bool fields_overlap(const ShaderDescriptorHeapFieldVK& p_first, const ShaderDescriptorHeapFieldVK& p_second) {
    constexpr uint32_t descriptor_index_size = sizeof(uint32_t) * 2;
    return p_first.offset < p_second.offset + descriptor_index_size && p_second.offset < p_first.offset + descriptor_index_size;
}

void append_descriptor_field(ShaderDescriptorHeapLayoutVK& p_layout, ShaderDescriptorHeapFieldVK p_field) {
    for (const ShaderDescriptorHeapFieldVK& existing : p_layout.fields) {
        if (existing.field_path == p_field.field_path) {
            throw std::runtime_error(
                std::format("Failed to resolve WBE_DESCRIPTOR: field path '{}' is declared more than once.", p_field.field_path));
        }
        if (fields_overlap(existing, p_field)) {
            throw std::runtime_error(std::format(
                "Failed to resolve WBE_DESCRIPTOR: fields '{}' and '{}' overlap in push data.", existing.field_path, p_field.field_path));
        }
    }
    p_layout.fields.push_back(std::move(p_field));
}

uint32_t collect_descriptor_heap_fields(const std::vector<ShaderMetadataTypeField>& p_fields,
    std::string_view p_parent_path,
    uint32_t p_parent_offset,
    ShaderDescriptorHeapLayoutVK& p_layout) {
    uint64_t extent = p_parent_offset;
    for (const ShaderMetadataTypeField& field : p_fields) {
        if (field.binding.offset < 0 || field.binding.size < 0) {
            throw std::runtime_error(std::format("Failed to resolve WBE_DESCRIPTOR: field '{}' has a negative layout value.", field.name));
        }
        uint64_t field_offset = static_cast<uint64_t>(p_parent_offset) + static_cast<uint32_t>(field.binding.offset);
        uint64_t field_extent = field_offset + static_cast<uint32_t>(field.binding.size);
        if (field_offset > std::numeric_limits<uint32_t>::max() || field_extent > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error(std::format("Failed to resolve WBE_DESCRIPTOR: field '{}' exceeds the supported push-data size.", field.name));
        }
        extent = std::max(extent, field_extent);

        std::string field_path = p_parent_path.empty() ? field.name : std::format("{}.{}", p_parent_path, field.name);
        const ShaderMetadataUserAttribute* attribute = find_descriptor_attribute(field);
        if (attribute != nullptr) {
            if (attribute->arguments.size() != 1 || attribute->arguments.front().kind != "string" ||
                attribute->arguments.front().string_value.empty()) {
                throw std::runtime_error(
                    std::format("Failed to resolve WBE_DESCRIPTOR: field '{}' requires one non-empty string argument.", field_path));
            }
            if (field.type.kind != "vector" || field.type.element_scalar_type != "uint32" || field.type.element_count != 2 ||
                field.binding.kind != "uniform" || field.binding.size != static_cast<int32_t>(sizeof(uint32_t) * 2) ||
                field_offset % (sizeof(uint32_t) * 2) != 0) {
                throw std::runtime_error(
                    std::format("Failed to resolve WBE_DESCRIPTOR: field '{}' must be an aligned eight-byte uint2.", field_path));
            }
            append_descriptor_field(p_layout,
                ShaderDescriptorHeapFieldVK{
                    .field_path = field_path,
                    .type = descriptor_heap_type_from_name(attribute->arguments.front().string_value),
                    .offset = static_cast<uint32_t>(field_offset),
                });
        }
        if (field.type.kind == "array" && field.type.element_count > 0 && !field.fields.empty()) {
            if (field.binding.element_stride <= 0) {
                throw std::runtime_error(
                    std::format("Failed to resolve WBE_DESCRIPTOR: descriptor struct array '{}' has no element stride.", field_path));
            }
            for (int32_t i = 0; i < field.type.element_count; ++i) {
                uint64_t element_offset = field_offset + static_cast<uint64_t>(i) * field.binding.element_stride;
                if (element_offset > std::numeric_limits<uint32_t>::max()) {
                    throw std::runtime_error(
                        std::format("Failed to resolve WBE_DESCRIPTOR: descriptor struct array '{}' exceeds push data.", field_path));
                }
                extent = std::max<uint64_t>(extent,
                    collect_descriptor_heap_fields(field.fields,
                        std::format("{}[{}]", field_path, i),
                        static_cast<uint32_t>(element_offset),
                        p_layout));
            }
        } else {
            extent = std::max<uint64_t>(
                extent, collect_descriptor_heap_fields(field.fields, field_path, static_cast<uint32_t>(field_offset), p_layout));
        }
    }
    return static_cast<uint32_t>(extent);
}

void merge_descriptor_heap_layout(ShaderDescriptorHeapLayoutVK& p_result, const ShaderDescriptorHeapLayoutVK& p_stage_layout) {
    p_result.size = std::max(p_result.size, p_stage_layout.size);
    p_result.stage_flags |= p_stage_layout.stage_flags;
    for (const ShaderDescriptorHeapFieldVK& field : p_stage_layout.fields) {
        auto existing = std::ranges::find_if(p_result.fields, [&field](const ShaderDescriptorHeapFieldVK& p_existing) {
            return p_existing.field_path == field.field_path;
        });
        if (existing != p_result.fields.end()) {
            if (existing->type != field.type || existing->offset != field.offset) {
                throw std::runtime_error(
                    std::format("Conflicting descriptor-heap push-data declarations for field '{}'.", field.field_path));
            }
            continue;
        }
        for (const ShaderDescriptorHeapFieldVK& other : p_result.fields) {
            if (fields_overlap(other, field)) {
                throw std::runtime_error(std::format(
                    "Conflicting descriptor-heap push-data fields '{}' and '{}' overlap.", other.field_path, field.field_path));
            }
        }
        p_result.fields.push_back(field);
    }
}

void append_stage_descriptor_heap_layout(ShaderDescriptorHeapLayoutVK& p_layout,
    const ShaderMetadata* p_metadata,
    std::string_view p_entry_name,
    std::string_view p_stage) {
    if (p_metadata == nullptr) {
        return;
    }
    std::string entry_name = p_entry_name.data() == nullptr ? std::string() : std::string(p_entry_name);
    merge_descriptor_heap_layout(p_layout, collect_shader_descriptor_heap_layout(*p_metadata, p_stage, entry_name));
}

} // namespace

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
    if (p_stage == "compute" || p_stage == "comp") {
        return VK_SHADER_STAGE_COMPUTE_BIT;
    }
    throw std::runtime_error(std::format("Unsupported shader stage '{}'.", p_stage));
}

VertexInputDescriptionVK build_vertex_input_description_vk(
    const ShaderMetadata& p_metadata, std::string_view p_entry_name, const RUVertexView& p_vertex_data) {
    const VertexInputContract contract = VertexInputContractResolver().resolve(p_metadata, p_entry_name, p_vertex_data);
    return VertexInputDescriptionVK::from_contract(contract);
}

VertexInputDescriptionVK build_vertex_input_description_vk(const ShaderMetadata& p_metadata,
    std::string_view p_entry_name,
    const RenderDatabase::OpaqueTrianglesInfo::VertexLayout& p_vertex_layout) {
    const VertexInputContract contract = VertexInputContractResolver().resolve(p_metadata, p_entry_name, p_vertex_layout);
    return VertexInputDescriptionVK::from_contract(contract);
}

ShaderDescriptorHeapLayoutVK collect_shader_descriptor_heap_layout(
    const ShaderMetadata& p_metadata, std::string_view p_stage, std::string_view p_entry_name) {
    const ShaderMetadataEntryPoint& entry_point = ShaderMetadataQuery().resolve_entry_point(p_metadata, p_stage, p_entry_name);
    const ShaderMetadataParameter* push_data = nullptr;
    for (const ShaderMetadataParameter& parameter : p_metadata.parameters) {
        if (parameter.binding.kind != "pushConstantBuffer" || !entry_point_uses_push_data(entry_point, parameter.binding)) {
            continue;
        }
        if (push_data != nullptr) {
            throw std::runtime_error(std::format("Shader entry point '{}': more than one push-data parameter is reflected.", entry_point.name));
        }
        push_data = &parameter;
    }
    ShaderDescriptorHeapLayoutVK result;
    if (push_data == nullptr) {
        return result;
    }
    if (push_data->type.kind != "constantBuffer") {
        throw std::runtime_error(
            std::format("Shader push-data parameter '{}': expected constantBuffer type metadata.", push_data->name));
    }
    result.stage_flags = shader_stage_flag_from_metadata_stage(entry_point.stage);
    result.size = collect_descriptor_heap_fields(push_data->type.fields, {}, 0, result);
    return result;
}

ShaderDescriptorHeapLayoutVK collect_graphics_pipeline_descriptor_heap_layout(const ShaderMetadata& p_vertex_shader_metadata,
    std::string_view p_vertex_stage_entry,
    const ShaderMetadata* p_geometry_shader_metadata,
    std::string_view p_geometry_stage_entry,
    const ShaderMetadata& p_fragment_shader_metadata,
    std::string_view p_fragment_stage_entry) {
    ShaderDescriptorHeapLayoutVK result;
    append_stage_descriptor_heap_layout(result, &p_vertex_shader_metadata, p_vertex_stage_entry, "vertex");
    append_stage_descriptor_heap_layout(result, p_geometry_shader_metadata, p_geometry_stage_entry, "geometry");
    append_stage_descriptor_heap_layout(result, &p_fragment_shader_metadata, p_fragment_stage_entry, "fragment");
    return result;
}

} // namespace WhiteBirdEngine
