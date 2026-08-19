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
#include "function/render_vk/render_vk_resource_unit_loader.hh"
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "core/parser/parser_json.hh"
#include "core/surface/3d_primitive.hh"
#include "function/render/render_resource_unit.hh"
#include "generated/serializables_access.gen.hh"
#include "generated/type_uuid.gen.hh"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "resource/resource_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {
namespace {

struct GeometrySlotInfo {
    std::string_view slot;
    std::string_view type;
    size_t component_count;
    size_t component_size;
    bool required;
};

struct GeometrySections {
    std::array<const RUSchemaSubmesh3DGeometrySection*, 5> attribute_sections{};
    const RUSchemaSubmesh3DGeometrySection* index_section = nullptr;
};

constexpr std::array<GeometrySlotInfo, 5> GEOMETRY_ATTRIBUTE_SLOT_INFOS{{
    {.slot = "position", .type = "vec3", .component_count = 3, .component_size = sizeof(float), .required = true},
    {.slot = "normal", .type = "vec3", .component_count = 3, .component_size = sizeof(float), .required = false},
    {.slot = "tangent", .type = "vec3", .component_count = 3, .component_size = sizeof(float), .required = false},
    {.slot = "bitangent", .type = "vec3", .component_count = 3, .component_size = sizeof(float), .required = false},
    {.slot = "uv", .type = "vec2", .component_count = 2, .component_size = sizeof(float), .required = false},
}};

constexpr GeometrySlotInfo GEOMETRY_INDEX_SLOT_INFO{
    .slot = "index", .type = "uint32", .component_count = 1, .component_size = sizeof(uint32_t), .required = true};

const GeometrySlotInfo* get_geometry_slot_info(std::string_view p_slot) {
    for (const auto& slot_info : GEOMETRY_ATTRIBUTE_SLOT_INFOS) {
        if (slot_info.slot == p_slot) {
            return &slot_info;
        }
    }
    if (GEOMETRY_INDEX_SLOT_INFO.slot == p_slot) {
        return &GEOMETRY_INDEX_SLOT_INFO;
    }
    return nullptr;
}

size_t get_geometry_attribute_slot_index(const GeometrySlotInfo& p_slot_info) {
    for (size_t i = 0; i < GEOMETRY_ATTRIBUTE_SLOT_INFOS.size(); ++i) {
        if (GEOMETRY_ATTRIBUTE_SLOT_INFOS[i].slot == p_slot_info.slot) {
            return i;
        }
    }
    throw std::runtime_error(std::format("Failed to populate submesh resource unit: unknown geometry slot {}.", p_slot_info.slot));
}

size_t get_geometry_element_size(const GeometrySlotInfo& p_slot_info) {
    return p_slot_info.component_count * p_slot_info.component_size;
}

size_t get_geometry_element_count(const RUSchemaSubmesh3DGeometrySection& p_section, const GeometrySlotInfo& p_slot_info) {
    return p_section.size / get_geometry_element_size(p_slot_info);
}

void validate_geometry_section_bounds(
    const RUSchemaSubmesh3DGeometrySection& p_section, const GeometrySlotInfo& p_slot_info, size_t p_geometry_size) {
    if (p_section.start > p_geometry_size || p_section.size > p_geometry_size - p_section.start) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry section '{}' range [{}, {}) exceeds file size {}.",
            p_section.slot,
            p_section.start,
            p_section.start + p_section.size,
            p_geometry_size));
    }

    if (p_section.start % p_slot_info.component_size != 0) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry section '{}' starts at unaligned byte offset {}.",
            p_section.slot,
            p_section.start));
    }

    const size_t element_size = get_geometry_element_size(p_slot_info);
    if (p_section.size % element_size != 0) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry section '{}' byte size {} is not a multiple of element size {}.",
            p_section.slot,
            p_section.size,
            element_size));
    }
}

GeometrySections validate_geometry_sections(const RUSchemaSubmesh3D& p_scheme, size_t p_geometry_size) {
    GeometrySections sections_by_slot{};
    for (const auto& section : p_scheme.geometry_sections) {
        const GeometrySlotInfo* slot_info = get_geometry_slot_info(section.slot);
        if (slot_info == nullptr) {
            throw std::runtime_error(
                std::format("Failed to populate submesh resource unit: unsupported geometry slot '{}'.", section.slot));
        }
        if (section.type != slot_info->type) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry slot '{}' expects type '{}' but got '{}'.",
                section.slot,
                slot_info->type,
                section.type));
        }

        if (slot_info->slot == GEOMETRY_INDEX_SLOT_INFO.slot) {
            if (sections_by_slot.index_section != nullptr) {
                throw std::runtime_error(
                    std::format("Failed to populate submesh resource unit: duplicate geometry section '{}'.", section.slot));
            }
            validate_geometry_section_bounds(section, *slot_info, p_geometry_size);
            sections_by_slot.index_section = &section;
            continue;
        }

        const size_t slot_index = get_geometry_attribute_slot_index(*slot_info);
        if (sections_by_slot.attribute_sections[slot_index] != nullptr) {
            throw std::runtime_error(
                std::format("Failed to populate submesh resource unit: duplicate geometry section '{}'.", section.slot));
        }
        validate_geometry_section_bounds(section, *slot_info, p_geometry_size);
        sections_by_slot.attribute_sections[slot_index] = &section;
    }

    for (size_t i = 0; i < GEOMETRY_ATTRIBUTE_SLOT_INFOS.size(); ++i) {
        if (GEOMETRY_ATTRIBUTE_SLOT_INFOS[i].required && sections_by_slot.attribute_sections[i] == nullptr) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: missing required geometry section '{}'.",
                GEOMETRY_ATTRIBUTE_SLOT_INFOS[i].slot));
        }
    }
    if (sections_by_slot.index_section == nullptr) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: missing required geometry section '{}'.",
            GEOMETRY_INDEX_SLOT_INFO.slot));
    }

    const auto* position_info = get_geometry_slot_info("position");
    const auto* position_section = sections_by_slot.attribute_sections[get_geometry_attribute_slot_index(*position_info)];
    const size_t vertex_count = get_geometry_element_count(*position_section, *get_geometry_slot_info("position"));
    for (size_t i = 0; i < GEOMETRY_ATTRIBUTE_SLOT_INFOS.size(); ++i) {
        if (sections_by_slot.attribute_sections[i] == nullptr || GEOMETRY_ATTRIBUTE_SLOT_INFOS[i].slot == "position") {
            continue;
        }
        const size_t attribute_count =
            get_geometry_element_count(*sections_by_slot.attribute_sections[i], GEOMETRY_ATTRIBUTE_SLOT_INFOS[i]);
        if (attribute_count != vertex_count) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry section '{}' has {} elements but position has {}.",
                GEOMETRY_ATTRIBUTE_SLOT_INFOS[i].slot,
                attribute_count,
                vertex_count));
        }
    }
    return sections_by_slot;
}

std::array<float, 3> read_geometry_vec3(
    const std::vector<uint8_t>& p_geometry_data, const RUSchemaSubmesh3DGeometrySection& p_section, size_t p_index) {
    std::array<float, 3> value{};
    std::memcpy(value.data(), p_geometry_data.data() + p_section.start + p_index * sizeof(value), sizeof(value));
    return value;
}

std::array<float, 2> read_geometry_vec2(
    const std::vector<uint8_t>& p_geometry_data, const RUSchemaSubmesh3DGeometrySection& p_section, size_t p_index) {
    std::array<float, 2> value{};
    std::memcpy(value.data(), p_geometry_data.data() + p_section.start + p_index * sizeof(value), sizeof(value));
    return value;
}

std::vector<uint32_t> build_indices_from_geometry_sections(
    const std::vector<uint8_t>& p_geometry_data, const RUSchemaSubmesh3DGeometrySection& p_index_section, size_t p_vertex_count);

template <typename TVertType>
void apply_geometry_vec3_section(const std::vector<uint8_t>& p_geometry_data,
    const RUSchemaSubmesh3DGeometrySection* p_section,
    std::vector<TVertType>& p_vertices,
    glm::vec3 TVertType::* p_member_ptr) {
    if (p_section == nullptr) {
        return;
    }
    for (size_t i = 0; i < p_vertices.size(); ++i) {
        const auto value = read_geometry_vec3(p_geometry_data, *p_section, i);
        p_vertices[i].*p_member_ptr = {value[0], value[1], value[2]};
    }
}

template <typename TVertType>
void apply_geometry_vec2_section(const std::vector<uint8_t>& p_geometry_data,
    const RUSchemaSubmesh3DGeometrySection* p_section,
    std::vector<TVertType>& p_vertices,
    glm::vec2 TVertType::* p_member_ptr) {
    if (p_section == nullptr) {
        return;
    }
    for (size_t i = 0; i < p_vertices.size(); ++i) {
        const auto value = read_geometry_vec2(p_geometry_data, *p_section, i);
        p_vertices[i].*p_member_ptr = {value[0], value[1]};
    }
}

template <typename TVertType>
bool vertex_supports_geometry_slot(std::string_view p_slot) {
    const auto field_names = SerializableAccess<TVertType>::list_fields();
    return std::find(field_names.begin(), field_names.end(), p_slot) != field_names.end();
}

template <typename TVertType>
void validate_vertex_type_sections(
    const std::array<const RUSchemaSubmesh3DGeometrySection*, GEOMETRY_ATTRIBUTE_SLOT_INFOS.size()>& p_sections_by_slot,
    std::string_view p_vertex_type_name) {
    for (size_t slot_index = 0; slot_index < GEOMETRY_ATTRIBUTE_SLOT_INFOS.size(); ++slot_index) {
        const auto& slot_info = GEOMETRY_ATTRIBUTE_SLOT_INFOS[slot_index];
        if (p_sections_by_slot[slot_index] == nullptr) {
            continue;
        }
        if (!vertex_supports_geometry_slot<TVertType>(slot_info.slot)) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: vertex type '{}' does not support geometry slot '{}'.",
                p_vertex_type_name,
                slot_info.slot));
        }
    }
}

template <typename TVertType>
std::vector<TVertType> build_vertices_from_geometry_sections(const std::vector<uint8_t>& p_geometry_data,
    const std::array<const RUSchemaSubmesh3DGeometrySection*, GEOMETRY_ATTRIBUTE_SLOT_INFOS.size()>& p_sections_by_slot) {
    const auto* position_info = get_geometry_slot_info("position");
    const auto* position_section = p_sections_by_slot[get_geometry_attribute_slot_index(*position_info)];
    std::vector<TVertType> vertices(get_geometry_element_count(*position_section, *position_info));
    SerializableAccess<TVertType> accessor;
    for (size_t slot_index = 0; slot_index < GEOMETRY_ATTRIBUTE_SLOT_INFOS.size(); ++slot_index) {
        const auto& slot_info = GEOMETRY_ATTRIBUTE_SLOT_INFOS[slot_index];
        const auto* section = p_sections_by_slot[slot_index];
        if (section == nullptr) {
            continue;
        }

        if (slot_info.type == "vec3") {
            auto member_ptr = accessor.template get_pointer_to_member<glm::vec3>(slot_info.slot);
            apply_geometry_vec3_section(p_geometry_data, section, vertices, member_ptr);
            continue;
        }

        if (slot_info.type == "vec2") {
            auto member_ptr = accessor.template get_pointer_to_member<glm::vec2>(slot_info.slot);
            apply_geometry_vec2_section(p_geometry_data, section, vertices, member_ptr);
            continue;
        }

        throw std::runtime_error(std::format("Failed to populate submesh resource unit: unsupported vertex attribute type '{}' for slot '{}'.",
            slot_info.type,
            slot_info.slot));
    }
    return vertices;
}

template <typename TVertType, typename TTriangleType>
void populate_submesh_triangles(IResourceChunk& p_res_chunk,
    const std::vector<uint8_t>& p_geometry_data,
    const GeometrySections& p_sections,
    RUSubmesh3D& p_resource_unit,
    std::string_view p_vertex_type_name) {
    validate_vertex_type_sections<TVertType>(p_sections.attribute_sections, p_vertex_type_name);
    auto vertices = build_vertices_from_geometry_sections<TVertType>(p_geometry_data, p_sections.attribute_sections);
    auto indices = build_indices_from_geometry_sections(p_geometry_data, *p_sections.index_section, vertices.size());

    auto vertices_data = p_res_chunk.push_data(vertices.size() * sizeof(TVertType), vertices.data());

    std::vector<TTriangleType> triangles(indices.size() / 3);
    for (size_t i = 0; i < triangles.size(); ++i) {
        auto& triangle = triangles.at(i);
        triangle.vert_array = vertices_data.template force_cast<TVertType>();
        triangle.vertex_indices[0] = indices.at(i * 3);
        triangle.vertex_indices[1] = indices.at(i * 3 + 1);
        triangle.vertex_indices[2] = indices.at(i * 3 + 2);
    }

    auto triangles_data = p_res_chunk.push_data(triangles.size() * sizeof(TTriangleType), triangles.data());
    p_resource_unit.triangle_type_uuid = TypeUUID<TTriangleType>::UUID;
    p_resource_unit.triangles.data = triangles_data.get();
    p_resource_unit.triangles.stride = sizeof(TTriangleType);
    p_resource_unit.triangles_count = triangles.size();
}

std::vector<uint32_t> build_indices_from_geometry_sections(
    const std::vector<uint8_t>& p_geometry_data, const RUSchemaSubmesh3DGeometrySection& p_index_section, size_t p_vertex_count) {
    const auto* index_info = &GEOMETRY_INDEX_SLOT_INFO;
    std::vector<uint32_t> indices(get_geometry_element_count(p_index_section, *index_info));
    if (!indices.empty()) {
        std::memcpy(indices.data(), p_geometry_data.data() + p_index_section.start, p_index_section.size);
    }
    if (indices.size() % 3 != 0) {
        throw std::runtime_error(
            std::format("Failed to populate submesh resource unit: index count {} is not a multiple of 3.", indices.size()));
    }
    for (uint32_t index : indices) {
        if (index >= p_vertex_count) {
            throw std::runtime_error(std::format(
                "Failed to populate submesh resource unit: index {} is out of range for {} vertices.", index, p_vertex_count));
        }
    }
    return indices;
}

template <typename SchemaType>
const SchemaType& get_typed_scheme(const RUSchema& p_scheme, std::string_view p_expected_type) {
    const auto* typed_scheme = dynamic_cast<const SchemaType*>(&p_scheme);
    if (typed_scheme == nullptr) {
        throw std::runtime_error(std::format("Failed to populate resource: expected typed scheme {} "
                                             "but got type {}.",
            p_expected_type,
            p_scheme.type));
    }
    return *typed_scheme;
}

RefRes<char> populate_text(IResourceChunk& p_res_chunk, std::string_view p_str) {
    RefRes<char> result = p_res_chunk.push_data(p_str.size() + 1, p_str.data());
    result.get()[p_str.size()] = '\0';
    return result;
}

RefRes<RUText> populate_text_resource(IResourceChunk& p_res_chunk, std::string_view p_str) {
    RUText resource_unit;
    resource_unit.text_data = populate_text(p_res_chunk, p_str);
    resource_unit.size = p_str.size() + 1;
    return p_res_chunk.push_res_unit("", resource_unit).force_cast<RUText>();
}

RefRes<RUImage> populate_image_resource(IResourceChunk& p_res_chunk, const RUSchemaImage& p_scheme) {
    if (p_scheme.path.empty()) {
        throw std::runtime_error("Failed to populate image resource: inline image requires path.");
    }
    ResourceUnitLoaderImage image_loader;
    return std::any_cast<RefRes<RUImage>>(
        static_cast<const IResourceUnitLoader&>(image_loader).populate_scheme_ptr(p_res_chunk, p_scheme));
}

RefRes<RUShader> populate_shader_resource(IResourceChunk& p_res_chunk, const RUSchemaShader& p_scheme) {
    auto file_path = get_resource_path(p_scheme.path);
    auto file_content = load_binary_file<uint32_t>(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size() * sizeof(decltype(*file_content.data())), file_content.data());
    RUShader resource_unit;
    resource_unit.metadata_path = populate_text(p_res_chunk, p_scheme.metadata);
    resource_unit.vertex_entry = populate_text(p_res_chunk, p_scheme.vertex_entry.value_or(""));
    resource_unit.tessellation_entry = populate_text(p_res_chunk, p_scheme.tessellation_entry.value_or(""));
    resource_unit.geometry_entry = populate_text(p_res_chunk, p_scheme.geometry_entry.value_or(""));
    resource_unit.fragment_entry = populate_text(p_res_chunk, p_scheme.fragment_entry.value_or(""));
    resource_unit.compute_entry = populate_text(p_res_chunk, p_scheme.compute_entry.value_or(""));
    resource_unit.data = data.force_cast<uint32_t>();
    resource_unit.size = file_content.size();
    return p_res_chunk.push_res_unit(p_scheme.id, resource_unit).force_cast<RUShader>();
}

void populate_pipeline_stage(IResourceChunk& p_res_chunk,
    RefRes<RUShader>& p_stage,
    const std::optional<std::string>& p_stage_id,
    const std::optional<RUSchemaShader>& p_stage_scheme,
    std::string_view p_stage_name,
    bool p_is_required = true) {
    if (p_stage_id.has_value()) {
        return;
    }
    if (!p_stage_scheme.has_value()) {
        if (!p_is_required) {
            return;
        }
        throw std::runtime_error(std::format("Failed to populate pipeline resource: {} stage "
                                             "requires either stage id or inline shader path.",
            p_stage_name));
    }
    p_stage = populate_shader_resource(p_res_chunk, p_stage_scheme.value());
}

void create_pipeline_stage_ref(IResourceChunk& p_res_chunk, RefRes<RUShader>& p_stage, const std::optional<std::string>& p_stage_id) {
    if (p_stage_id.has_value()) {
        p_res_chunk.create_ref_to(p_stage, p_stage_id.value());
    }
}

RefRes<char> push_stage_entry(IResourceChunk& p_res_chunk, std::string_view p_entry) {
    std::string stage_entry(p_entry);
    return p_res_chunk.push_data(stage_entry.size() + 1, stage_entry.c_str());
}

std::string_view resolve_pipeline_stage_entry(const std::optional<std::string>& p_stage_entry, bool p_stage_present) {
    if (p_stage_entry.has_value()) {
        return p_stage_entry.value();
    }
    if (!p_stage_present) {
        return {};
    }
    // Leave the pipeline entry unset so compile-time resolution can fall back to the shader resource defaults
    // or to the sole reflected entry point for the stage.
    return {};
}

bool scheme_has_pipeline_stage(const std::optional<std::string>& p_stage_id, const std::optional<RUSchemaShader>& p_stage_scheme) {
    return p_stage_id.has_value() || p_stage_scheme.has_value();
}

RefRes<RUGraphicsPipeline> populate_graphics_pipeline_resource(IResourceChunk& p_res_chunk, const RUSchemaGraphicsPipeline& p_scheme) {
    RUGraphicsPipeline resource_unit;
    populate_pipeline_stage(p_res_chunk, resource_unit.vertex_stage, p_scheme.vertex_stage_id, p_scheme.vertex_stage, "vertex");
    populate_pipeline_stage(
        p_res_chunk, resource_unit.tessellation_stage, p_scheme.tessellation_stage_id, p_scheme.tessellation_stage, "tessellation", false);
    populate_pipeline_stage(
        p_res_chunk, resource_unit.geometry_stage, p_scheme.geometry_stage_id, p_scheme.geometry_stage, "geometry", false);
    populate_pipeline_stage(p_res_chunk, resource_unit.fragment_stage, p_scheme.fragment_stage_id, p_scheme.fragment_stage, "fragment");
    resource_unit.vertex_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(
            p_scheme.vertex_stage_entry, scheme_has_pipeline_stage(p_scheme.vertex_stage_id, p_scheme.vertex_stage)));
    resource_unit.tessellation_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(p_scheme.tessellation_stage_entry,
            scheme_has_pipeline_stage(p_scheme.tessellation_stage_id, p_scheme.tessellation_stage)));
    resource_unit.geometry_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(
            p_scheme.geometry_stage_entry, scheme_has_pipeline_stage(p_scheme.geometry_stage_id, p_scheme.geometry_stage)));
    resource_unit.fragment_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(
            p_scheme.fragment_stage_entry, scheme_has_pipeline_stage(p_scheme.fragment_stage_id, p_scheme.fragment_stage)));
    resource_unit.depth_test = p_scheme.depth_test;
    resource_unit.depth_write = p_scheme.depth_write;
    resource_unit.cull_backfaces = p_scheme.cull_backfaces;
    auto saved_resource = p_res_chunk.push_res_unit(p_scheme.id, resource_unit).force_cast<RUGraphicsPipeline>();
    create_pipeline_stage_ref(p_res_chunk, saved_resource->vertex_stage, p_scheme.vertex_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->tessellation_stage, p_scheme.tessellation_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->geometry_stage, p_scheme.geometry_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->fragment_stage, p_scheme.fragment_stage_id);
    return saved_resource;
}

RefRes<RUMaterial> resolve_material(IResourceChunk& p_res_chunk, const RUSchemaMaterial& p_material) {
    const bool has_inline_pipeline = p_material.graphics_pipeline.has_value();
    if (p_material.graphics_pipeline_ids.empty() && !has_inline_pipeline) {
        throw std::runtime_error("Failed to populate material resource: graphics_pipeline_ids or inline graphics_pipeline is required.");
    }
    if (!p_material.graphics_pipeline_ids.empty() && has_inline_pipeline) {
        throw std::runtime_error("Failed to populate material resource: graphics pipeline is declared more than once.");
    }

    RUMaterial resource_unit;
    if (has_inline_pipeline) {
        resource_unit.graphics_pipeline = populate_graphics_pipeline_resource(p_res_chunk, p_material.graphics_pipeline.value());
    }
    std::vector<RUMaterial::TextureSlot> texture_slots;
    std::vector<std::string> texture_ids;
    texture_slots.reserve(p_material.textures.size());
    texture_ids.reserve(p_material.textures.size());
    for (const auto& texture_scheme : p_material.textures) {
        if (texture_scheme.texture_key.empty()) {
            throw std::runtime_error("Failed to populate material resource: texture slot requires non-empty texture_key.");
        }
        RUMaterial::TextureSlot texture_slot;
        texture_slot.texture_key = populate_text_resource(p_res_chunk, texture_scheme.texture_key);
        if (texture_scheme.texture_id.has_value()) {
            texture_ids.push_back(texture_scheme.texture_id.value());
        } else {
            if (!texture_scheme.texture.has_value()) {
                throw std::runtime_error("Failed to populate material resource: texture slot requires either texture_id or inline texture.");
            }
            texture_slot.texture = populate_image_resource(p_res_chunk, texture_scheme.texture.value());
            texture_ids.emplace_back();
        }
        texture_slots.push_back(texture_slot);
    }

    resource_unit.texture_count = texture_slots.size();
    if (!texture_slots.empty()) {
        auto texture_slot_data = p_res_chunk.push_data(texture_slots.size() * sizeof(RUMaterial::TextureSlot), texture_slots.data());
        resource_unit.textures = texture_slot_data.force_cast<RUMaterial::TextureSlot>();
    }

    auto saved_resource = p_res_chunk.push_res_unit(p_material.id, resource_unit).force_cast<RUMaterial>();
    if (!p_material.graphics_pipeline_ids.empty()) {
        p_res_chunk.create_ref_to(saved_resource->graphics_pipeline, p_material.graphics_pipeline_ids.front());
    }
    auto* saved_texture_slots = saved_resource->textures.get();
    for (size_t i = 0; i < texture_ids.size(); ++i) {
        if (!texture_ids[i].empty()) {
            p_res_chunk.create_ref_to(saved_texture_slots[i].texture, texture_ids[i]);
        }
    }
    return saved_resource;
}

void resolve_submesh_data(IResourceChunk& p_res_chunk, const RUSchemaSubmesh3D& p_scheme, RUSubmesh3D& p_resource_unit) {
    if (!p_scheme.material_id.has_value()) {
        if (!p_scheme.material.has_value()) {
            throw std::runtime_error("Failed to populate submesh resource unit: material_id or inline material is required.");
        }
        p_resource_unit.material = resolve_material(p_res_chunk, p_scheme.material.value());
    }

    if (p_scheme.geometry_path.empty()) {
        throw std::runtime_error("Failed to populate submesh resource unit: geometry_path is required.");
    }
    auto geometry_path = get_resource_path(p_scheme.geometry_path);
    auto geometry_data = load_binary_file<uint8_t>(static_cast<std::string>(geometry_path).c_str());
    const auto sections_by_slot = validate_geometry_sections(p_scheme, geometry_data.size());
    TypeUUIDManager* type_uuid_manager = TypeUUIDManager::get_singleton();
    if (type_uuid_manager == nullptr) {
        throw std::runtime_error("Failed to populate submesh resource unit: TypeUUIDManager is not initialized.");
    }

    const TypeID vertex_type_uuid = type_uuid_manager->get_type_uuid(p_scheme.vertex_type);
    switch (vertex_type_uuid) {
    case TypeUUID<Vertex3D>::UUID:
        populate_submesh_triangles<Vertex3D, Triangle3DIndxVertex3D>(
            p_res_chunk, geometry_data, sections_by_slot, p_resource_unit, p_scheme.vertex_type);
        break;
    case TypeUUID<Vertex3DTextured>::UUID:
        populate_submesh_triangles<Vertex3DTextured, Triangle3DIndxTextured>(
            p_res_chunk, geometry_data, sections_by_slot, p_resource_unit, p_scheme.vertex_type);
        break;
    default:
        throw std::runtime_error(
            std::format("Failed to populate submesh resource unit: unsupported vertex type '{}'.", p_scheme.vertex_type));
    }
}

void create_submesh_material_ref(IResourceChunk& p_res_chunk, RUSubmesh3D& p_resource_unit, const RUSchemaSubmesh3D& p_scheme) {
    if (p_scheme.material_id.has_value()) {
        p_res_chunk.create_ref_to(p_resource_unit.material, p_scheme.material_id.value());
    }
}

} // namespace

std::span<const std::string> ResourceUnitLoaderShader::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"shader"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderShader::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaShader>(p_scheme, "shader");
    return populate_shader_resource(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderShader::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaShader scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderGraphicsPipeline::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"graphics_pipeline"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderGraphicsPipeline::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaGraphicsPipeline>(p_scheme, "graphics_pipeline");
    return populate_graphics_pipeline_resource(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderGraphicsPipeline::populate_scheme_from_data(
    IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaGraphicsPipeline scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderMaterial::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"material"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderMaterial::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaMaterial>(p_scheme, "material");
    return resolve_material(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderMaterial::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaMaterial scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderMesh::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"mesh"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderMesh::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaMesh3D>(p_scheme, "mesh");
    RUMesh3D resource_unit;
    std::vector<RefRes<RUSubmesh3D>> submeshes;
    submeshes.reserve(typed_scheme.submeshes.size() + typed_scheme.submeshes_id.size());
    for (const auto& submesh_scheme : typed_scheme.submeshes) {
        RUSubmesh3D submesh;
        resolve_submesh_data(p_res_chunk, submesh_scheme, submesh);
        auto saved_resource = p_res_chunk.push_res_unit("", submesh).force_cast<RUSubmesh3D>();
        WBE_DEBUG_ASSERT(saved_resource != nullptr);
        create_submesh_material_ref(p_res_chunk, *saved_resource, submesh_scheme);
        submeshes.push_back(saved_resource);
    }
    // TODO: resolve submesh id
    size_t submesh_index_array_size_bytes = submeshes.size() * sizeof(RefRes<RUSubmesh3D>);
    auto submesh_array = p_res_chunk.push_data(submesh_index_array_size_bytes, submeshes.data()).force_cast<RefRes<RUSubmesh3D>>();
    resource_unit.submeshes = submesh_array;
    resource_unit.submesh_count = submeshes.size();
    return p_res_chunk.push_res_unit(typed_scheme.id, resource_unit).force_cast<RUMesh3D>();
}

void ResourceUnitLoaderMesh::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaMesh3D scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderSubmesh::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"submesh"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderSubmesh::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaSubmesh3D>(p_scheme, "submesh");
    RUSubmesh3D resource_unit;
    resolve_submesh_data(p_res_chunk, typed_scheme, resource_unit);
    auto saved_resource = p_res_chunk.push_res_unit(typed_scheme.id, resource_unit).force_cast<RUSubmesh3D>();
    create_submesh_material_ref(p_res_chunk, *saved_resource, typed_scheme);
    return saved_resource;
}

void ResourceUnitLoaderSubmesh::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaSubmesh3D scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

void register_renderer_vk_resource_unit_loaders() {
    auto* resource_manager = ResourceManager::get_singleton();
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderShader>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderGraphicsPipeline>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderMaterial>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderMesh>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderSubmesh>(global_allocator()));
}

} // namespace WhiteBirdEngine
