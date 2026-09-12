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
#include "function/render/static_set_node_contract_resolver.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_query.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {
namespace {

const ShaderMetadataTypeField& require_field(const std::vector<ShaderMetadataTypeField>& p_fields,
    std::string_view p_name,
    std::string_view p_context) {
    const ShaderMetadataTypeField* field = ShaderMetadataQuery().find_field(p_fields, p_name);
    if (field == nullptr) {
        throw std::runtime_error(std::format("Failed to resolve static-set node contract: {} field '{}' is missing.", p_context, p_name));
    }
    return *field;
}

size_t resolve_field_extent(const ShaderMetadataTypeField& p_field) {
    if (p_field.binding.offset < 0 || p_field.binding.size <= 0) {
        throw std::runtime_error(std::format(
            "Failed to resolve static-set node contract: field '{}' has an invalid reflected layout.", p_field.name));
    }
    return static_cast<size_t>(p_field.binding.offset) + static_cast<size_t>(p_field.binding.size);
}

} // namespace

StaticSetNodeContract StaticSetNodeContractResolver::resolve(
    const ShaderMetadata& p_metadata, std::string_view p_static_set_parameter_name) const {
    if (p_static_set_parameter_name.empty()) {
        throw std::runtime_error("Failed to resolve static-set node contract: parameter name is empty.");
    }

    ShaderMetadataQuery metadata_query;
    const ShaderMetadataParameter* static_set_parameter = metadata_query.find_parameter(p_metadata, p_static_set_parameter_name);
    if (static_set_parameter == nullptr) {
        throw std::runtime_error(std::format(
            "Failed to resolve static-set node contract: parameter '{}' is missing.", p_static_set_parameter_name));
    }
    if (static_set_parameter->type.kind != "parameterBlock") {
        throw std::runtime_error(std::format(
            "Failed to resolve static-set node contract: parameter '{}' must be a parameterBlock.", p_static_set_parameter_name));
    }

    const ShaderMetadataTypeField& nodes_field = require_field(static_set_parameter->type.fields, "nodes", "static_set parameter");
    if (nodes_field.type.kind != "resource" || nodes_field.type.scalar_type != "structuredBuffer") {
        throw std::runtime_error("Failed to resolve static-set node contract: static_set.nodes must be a StructuredBuffer resource.");
    }
    if (nodes_field.fields.empty()) {
        throw std::runtime_error(
            "Failed to resolve static-set node contract: static_set.nodes is missing reflected NodeData fields.");
    }

    const ShaderMetadataTypeField& transform_field = require_field(nodes_field.fields, "transform", "NodeData");
    const ShaderMetadataTypeField& normal_transform_field =
        require_field(nodes_field.fields, "normal_transform", "NodeData");

    size_t element_stride = 0;
    for (const ShaderMetadataTypeField& field : nodes_field.fields) {
        element_stride = std::max(element_stride, resolve_field_extent(field));
    }

    StaticSetNodeContract result;
    result.element_stride = element_stride;
    result.transform_offset = static_cast<size_t>(transform_field.binding.offset);
    result.normal_transform_offset = static_cast<size_t>(normal_transform_field.binding.offset);
    return result;
}

} // namespace WhiteBirdEngine