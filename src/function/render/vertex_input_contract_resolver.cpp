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
#include "function/render/vertex_input_contract_resolver.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_query.hh"
#include "function/render/shader_role_field_resolver.hh"
#include "function/render/vertex_attribute_format_info.hh"
#include "function/render/vertex_input_contract.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

struct VertexAttributeLayoutView {
    std::string_view role_name;
    HashCode role_hash = 0;
    RUVertexAttributeFormat format = RUVertexAttributeFormat::FLOAT;
    size_t offset = 0;
    size_t size = 0;
};

struct VertexLayoutView {
    std::vector<VertexAttributeLayoutView> attributes;
    size_t vertex_stride = 0;
};

static VertexLayoutView make_vertex_layout_view(const RUVertexView& p_vertex_data) {
    if (p_vertex_data.attribute_count > 0 && p_vertex_data.attributes == nullptr) {
        throw std::runtime_error("Failed to resolve vertex input contract: resource attribute metadata is missing.");
    }
    VertexLayoutView result;
    result.vertex_stride = p_vertex_data.vertex_stride;
    result.attributes.reserve(p_vertex_data.attribute_count);
    for (size_t i = 0; i < p_vertex_data.attribute_count; ++i) {
        const RUVertexView::Attribute& attribute = p_vertex_data.attributes.get()[i];
        result.attributes.push_back(VertexAttributeLayoutView{.role_name = attribute.role.get_name(),
            .role_hash = attribute.role.hash,
            .format = attribute.format,
            .offset = attribute.offset,
            .size = attribute.size});
    }
    return result;
}

static void validate_vertex_layout(const VertexLayoutView& p_vertex_layout) {
    size_t expected_offset = 0;
    for (size_t i = 0; i < p_vertex_layout.attributes.size(); ++i) {
        const VertexAttributeLayoutView& attribute = p_vertex_layout.attributes[i];
        if (attribute.role_name.empty() || attribute.role_name.contains('\0')) {
            throw std::runtime_error("Failed to resolve vertex input contract: resource attribute role is invalid.");
        }
        if (attribute.role_hash != dynam_hash(attribute.role_name)) {
            throw std::runtime_error(std::format(
                "Failed to resolve vertex input contract: resource role '{}' has an invalid hash.", attribute.role_name));
        }
        if (attribute.size != VertexAttributeFormatInfo::get_size(attribute.format) || attribute.offset != expected_offset ||
            attribute.size > std::numeric_limits<size_t>::max() - expected_offset) {
            throw std::runtime_error(std::format(
                "Failed to resolve vertex input contract: resource role '{}' has an invalid layout.", attribute.role_name));
        }
        for (size_t previous_index = 0; previous_index < i; ++previous_index) {
            const VertexAttributeLayoutView& previous = p_vertex_layout.attributes[previous_index];
            if (previous.role_name == attribute.role_name) {
                throw std::runtime_error(std::format(
                    "Failed to resolve vertex input contract: resource role '{}' is duplicated.", attribute.role_name));
            }
            if (previous.role_hash == attribute.role_hash) {
                throw std::runtime_error(std::format("Failed to resolve vertex input contract: distinct resource roles '{}' and '{}' collide at hash {}.",
                    previous.role_name,
                    attribute.role_name,
                    attribute.role_hash));
            }
        }
        expected_offset += attribute.size;
    }
    if (expected_offset != p_vertex_layout.vertex_stride) {
        throw std::runtime_error("Failed to resolve vertex input contract: resource attributes are not tightly packed to the declared stride.");
    }
}

static RUVertexAttributeFormat reflect_vertex_format(const ShaderMetadataTypeAtom& p_type, std::string_view p_field_name) {
    std::string_view scalar_type;
    int32_t component_count = 0;
    if (p_type.kind == "scalar") {
        scalar_type = p_type.scalar_type;
        component_count = 1;
    } else if (p_type.kind == "vector") {
        scalar_type = p_type.element_scalar_type;
        component_count = p_type.element_count;
    } else {
        throw std::runtime_error(std::format("Failed to resolve vertex input contract: WBE_VERTEX_INPUT field '{}' has unsupported shader type '{}'.",
            p_field_name,
            p_type.kind));
    }
    if (component_count < 1 || component_count > 4) {
        throw std::runtime_error(std::format("Failed to resolve vertex input contract: WBE_VERTEX_INPUT field '{}' has unsupported component count {}.",
            p_field_name,
            component_count));
    }
    const size_t index = static_cast<size_t>(component_count - 1);
    if (scalar_type == "float32") {
        constexpr RUVertexAttributeFormat FORMATS[] = {
            RUVertexAttributeFormat::FLOAT, RUVertexAttributeFormat::FLOAT2, RUVertexAttributeFormat::FLOAT3, RUVertexAttributeFormat::FLOAT4};
        return FORMATS[index];
    }
    if (scalar_type == "int32") {
        constexpr RUVertexAttributeFormat FORMATS[] = {
            RUVertexAttributeFormat::INT, RUVertexAttributeFormat::INT2, RUVertexAttributeFormat::INT3, RUVertexAttributeFormat::INT4};
        return FORMATS[index];
    }
    if (scalar_type == "uint32") {
        constexpr RUVertexAttributeFormat FORMATS[] = {
            RUVertexAttributeFormat::UINT, RUVertexAttributeFormat::UINT2, RUVertexAttributeFormat::UINT3, RUVertexAttributeFormat::UINT4};
        return FORMATS[index];
    }
    throw std::runtime_error(std::format(
        "Failed to resolve vertex input contract: WBE_VERTEX_INPUT field '{}' has unsupported scalar type '{}'.", p_field_name, scalar_type));
}

static const VertexAttributeLayoutView& find_vertex_attribute(const VertexLayoutView& p_vertex_layout, const ShaderRole& p_role) {
    const VertexAttributeLayoutView* result = nullptr;
    for (const VertexAttributeLayoutView& attribute : p_vertex_layout.attributes) {
        if (!p_role.matches(attribute.role_name, attribute.role_hash)) {
            continue;
        }
        if (result != nullptr) {
            throw std::runtime_error(
                std::format("Failed to resolve vertex input contract: resource role '{}' is duplicated.", p_role.get_name()));
        }
        result = &attribute;
    }
    if (result == nullptr) {
        throw std::runtime_error(
            std::format("Failed to resolve vertex input contract: resource role '{}' is missing.", p_role.get_name()));
    }
    return *result;
}

static VertexInputContract resolve_vertex_input_contract(
    const ShaderMetadata& p_metadata, std::string_view p_entry_name, const VertexLayoutView& p_vertex_layout) {
    validate_vertex_layout(p_vertex_layout);
    const ShaderMetadataEntryPoint& entry_point = ShaderMetadataQuery().resolve_entry_point(p_metadata, "vertex", p_entry_name);
    ShaderRoleFieldResolver role_resolver;
    VertexInputContract result;
    result.vertex_stride = p_vertex_layout.vertex_stride;
    for (const ShaderMetadataParameter& parameter : entry_point.parameters) {
        std::vector<ShaderRoleFieldResolver::Field> fields =
            role_resolver.collect_fields(parameter.type.fields, "WBE_VERTEX_INPUT", ShaderRoleFieldResolver::Traversal::RECURSIVE);
        for (ShaderRoleFieldResolver::Field& field : fields) {
            if (field.metadata->binding.kind != "varyingInput" || field.metadata->binding.index < 0) {
                throw std::runtime_error(std::format("Failed to resolve vertex input contract: WBE_VERTEX_INPUT field '{}' is not a located varying input.",
                    field.metadata->name));
            }
            const uint32_t location = static_cast<uint32_t>(field.metadata->binding.index);
            for (const VertexInputContract::Attribute& existing : result.attributes) {
                if (existing.role.get_name() == field.role.get_name()) {
                    throw std::runtime_error(std::format(
                        "Failed to resolve vertex input contract: shader role '{}' is duplicated.", field.role.get_name()));
                }
                if (existing.role.get_hash() == field.role.get_hash()) {
                    throw std::runtime_error(std::format("Failed to resolve vertex input contract: shader roles '{}' and '{}' collide at hash {}.",
                        existing.role.get_name(),
                        field.role.get_name(),
                        field.role.get_hash()));
                }
                if (existing.location == location) {
                    throw std::runtime_error(
                        std::format("Failed to resolve vertex input contract: shader location {} is duplicated.", location));
                }
            }
            const RUVertexAttributeFormat format = reflect_vertex_format(field.metadata->type, field.metadata->name);
            const VertexAttributeLayoutView& resource_attribute = find_vertex_attribute(p_vertex_layout, field.role);
            if (resource_attribute.format != format) {
                throw std::runtime_error(std::format("Failed to resolve vertex input contract: shader role '{}' is incompatible with the resource format.",
                    field.role.get_name()));
            }
            result.attributes.push_back(VertexInputContract::Attribute{
                .role = std::move(field.role), .format = format, .location = location, .offset = resource_attribute.offset});
        }
    }
    std::sort(result.attributes.begin(), result.attributes.end(), [](const auto& p_left, const auto& p_right) {
        return p_left.location < p_right.location;
    });
    return result;
}

VertexInputContract VertexInputContractResolver::resolve(
    const ShaderMetadata& p_metadata, std::string_view p_entry_name, const RUVertexView& p_vertex_data) const {
    return resolve_vertex_input_contract(p_metadata, p_entry_name, make_vertex_layout_view(p_vertex_data));
}

} // namespace WhiteBirdEngine
