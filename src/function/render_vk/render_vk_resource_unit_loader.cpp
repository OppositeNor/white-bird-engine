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
#include "core/allocator/i_allocator.hh"
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "core/parser/parser_json.hh"
#include "function/render/render_resource_unit.hh"
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
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {
namespace {

struct GeometryFormatInfo {
    std::string_view type;
    RUVertexAttributeFormat format;
    size_t size;
};

struct GeometryAttributeSection {
    const RUSchemaSubmesh3DGeometrySection* section = nullptr;
    const GeometryFormatInfo* format = nullptr;
    size_t offset = 0;
};

struct GeometrySections {
    std::vector<GeometryAttributeSection> attribute_sections;
    const RUSchemaSubmesh3DGeometrySection* index_section = nullptr;
    size_t vertex_stride = 0;
    size_t vertex_count = 0;
};

constexpr std::array<GeometryFormatInfo, 12> GEOMETRY_ATTRIBUTE_FORMAT_INFOS{{
    {.type = "float", .format = RUVertexAttributeFormat::FLOAT, .size = sizeof(float)},
    {.type = "vec2", .format = RUVertexAttributeFormat::FLOAT2, .size = sizeof(float) * 2},
    {.type = "vec3", .format = RUVertexAttributeFormat::FLOAT3, .size = sizeof(float) * 3},
    {.type = "vec4", .format = RUVertexAttributeFormat::FLOAT4, .size = sizeof(float) * 4},
    {.type = "int", .format = RUVertexAttributeFormat::INT, .size = sizeof(int32_t)},
    {.type = "ivec2", .format = RUVertexAttributeFormat::INT2, .size = sizeof(int32_t) * 2},
    {.type = "ivec3", .format = RUVertexAttributeFormat::INT3, .size = sizeof(int32_t) * 3},
    {.type = "ivec4", .format = RUVertexAttributeFormat::INT4, .size = sizeof(int32_t) * 4},
    {.type = "uint", .format = RUVertexAttributeFormat::UINT, .size = sizeof(uint32_t)},
    {.type = "uvec2", .format = RUVertexAttributeFormat::UINT2, .size = sizeof(uint32_t) * 2},
    {.type = "uvec3", .format = RUVertexAttributeFormat::UINT3, .size = sizeof(uint32_t) * 3},
    {.type = "uvec4", .format = RUVertexAttributeFormat::UINT4, .size = sizeof(uint32_t) * 4},
}};

constexpr std::string_view GEOMETRY_INDEX_SLOT = "index";
constexpr std::string_view GEOMETRY_INDEX_TYPE = "uint32";

const GeometryFormatInfo* get_geometry_format_info(std::string_view p_type) {
    for (const auto& format_info : GEOMETRY_ATTRIBUTE_FORMAT_INFOS) {
        if (format_info.type == p_type) {
            return &format_info;
        }
    }
    return nullptr;
}

size_t get_geometry_element_count(const RUSchemaSubmesh3DGeometrySection& p_section, size_t p_element_size) {
    return p_section.size / p_element_size;
}

void validate_geometry_section_bounds(const RUSchemaSubmesh3DGeometrySection& p_section, size_t p_element_size, size_t p_geometry_size) {
    if (p_section.start > p_geometry_size || p_section.size > p_geometry_size - p_section.start) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry section '{}' range [{}, {}) exceeds file size {}.",
            p_section.slot,
            p_section.start,
            p_section.start + p_section.size,
            p_geometry_size));
    }

    if (p_section.start % sizeof(uint32_t) != 0) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry section '{}' starts at unaligned byte offset {}.",
            p_section.slot,
            p_section.start));
    }

    if (p_section.size % p_element_size != 0) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry section '{}' byte size {} is not a multiple of element size {}.",
            p_section.slot,
            p_section.size,
            p_element_size));
    }
}

GeometrySections validate_geometry_sections(const RUSchemaSubmesh3D& p_scheme, size_t p_geometry_size) {
    GeometrySections sections_by_slot{};
    for (const auto& section : p_scheme.geometry_sections) {
        if (section.slot.empty() || section.slot.contains('\0')) {
            throw std::runtime_error(
                "Failed to populate submesh resource unit: geometry section role must be non-empty and must not contain NUL.");
        }
        if (section.slot == GEOMETRY_INDEX_SLOT) {
            if (sections_by_slot.index_section != nullptr) {
                throw std::runtime_error(
                    std::format("Failed to populate submesh resource unit: duplicate geometry section '{}'.", section.slot));
            }
            if (section.type != GEOMETRY_INDEX_TYPE) {
                throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry slot '{}' expects type '{}' but got '{}'.",
                    section.slot,
                    GEOMETRY_INDEX_TYPE,
                    section.type));
            }
            validate_geometry_section_bounds(section, sizeof(uint32_t), p_geometry_size);
            sections_by_slot.index_section = &section;
            continue;
        }

        if (std::ranges::any_of(sections_by_slot.attribute_sections, [&section](const GeometryAttributeSection& p_existing) {
                return p_existing.section->slot == section.slot;
            })) {
            throw std::runtime_error(
                std::format("Failed to populate submesh resource unit: duplicate geometry section '{}'.", section.slot));
        }
        if (std::ranges::any_of(sections_by_slot.attribute_sections, [&section](const GeometryAttributeSection& p_existing) {
                return dynam_hash(p_existing.section->slot) == dynam_hash(section.slot);
            })) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: distinct geometry section roles collide at hash {}.",
                dynam_hash(section.slot)));
        }
        const GeometryFormatInfo* format_info = get_geometry_format_info(section.type);
        if (format_info == nullptr) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: unsupported vertex attribute type '{}' for role '{}'.",
                section.type,
                section.slot));
        }
        validate_geometry_section_bounds(section, format_info->size, p_geometry_size);

        const size_t attribute_count = get_geometry_element_count(section, format_info->size);
        if (sections_by_slot.attribute_sections.empty()) {
            sections_by_slot.vertex_count = attribute_count;
        } else if (attribute_count != sections_by_slot.vertex_count) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: geometry section '{}' has {} elements but the first vertex attribute has {}.",
                section.slot,
                attribute_count,
                sections_by_slot.vertex_count));
        }
        if (format_info->size > std::numeric_limits<size_t>::max() - sections_by_slot.vertex_stride) {
            throw std::runtime_error("Failed to populate submesh resource unit: vertex stride exceeds the supported size.");
        }
        sections_by_slot.attribute_sections.push_back(
            GeometryAttributeSection{.section = &section, .format = format_info, .offset = sections_by_slot.vertex_stride});
        sections_by_slot.vertex_stride += format_info->size;
    }

    if (sections_by_slot.attribute_sections.empty()) {
        throw std::runtime_error("Failed to populate submesh resource unit: at least one vertex attribute geometry section is required.");
    }
    if (sections_by_slot.index_section == nullptr) {
        throw std::runtime_error(
            std::format("Failed to populate submesh resource unit: missing required geometry section '{}'.", GEOMETRY_INDEX_SLOT));
    }
    return sections_by_slot;
}

std::vector<uint32_t> build_indices_from_geometry_sections(
    const std::vector<uint8_t>& p_geometry_data, const RUSchemaSubmesh3DGeometrySection& p_index_section, size_t p_vertex_count);

std::vector<uint8_t> build_interleaved_vertices(const std::vector<uint8_t>& p_geometry_data, const GeometrySections& p_sections) {
    if (p_sections.vertex_stride != 0 && p_sections.vertex_count > std::numeric_limits<size_t>::max() / p_sections.vertex_stride) {
        throw std::runtime_error("Failed to populate submesh resource unit: interleaved vertex data exceeds the supported size.");
    }
    std::vector<uint8_t> vertices(p_sections.vertex_count * p_sections.vertex_stride);
    for (const GeometryAttributeSection& attribute_section : p_sections.attribute_sections) {
        for (size_t vertex_index = 0; vertex_index < p_sections.vertex_count; ++vertex_index) {
            std::memcpy(vertices.data() + vertex_index * p_sections.vertex_stride + attribute_section.offset,
                p_geometry_data.data() + attribute_section.section->start + vertex_index * attribute_section.format->size,
                attribute_section.format->size);
        }
    }
    return vertices;
}

RUShaderRole populate_shader_role(IResourceChunk& p_res_chunk, std::string_view p_role_name) {
    if (p_role_name.empty() || p_role_name.contains('\0')) {
        throw std::runtime_error("Failed to populate shader role: role name must be non-empty and must not contain NUL.");
    }
    const std::string retained_name(p_role_name);
    return RUShaderRole(p_res_chunk.push_data(retained_name.size() + 1, retained_name.c_str()), dynam_hash(p_role_name));
}

RefRes<RUVertexData> populate_vertex_data(
    IResourceChunk& p_res_chunk, const std::vector<uint8_t>& p_geometry_data, const GeometrySections& p_sections) {
    std::vector<uint8_t> vertices = build_interleaved_vertices(p_geometry_data, p_sections);
    RefRes<uint8_t> saved_vertices = MEM_NULL;
    if (!vertices.empty()) {
        saved_vertices = p_res_chunk.push_data(vertices.size(), vertices.data()).force_cast<uint8_t>();
    }

    std::vector<RUVertexData::Attribute> attributes;
    attributes.reserve(p_sections.attribute_sections.size());
    for (const GeometryAttributeSection& attribute_section : p_sections.attribute_sections) {
        const std::string& role = attribute_section.section->slot;
        attributes.push_back(RUVertexData::Attribute{.role = populate_shader_role(p_res_chunk, role),
            .format = attribute_section.format->format,
            .offset = attribute_section.offset,
            .size = attribute_section.format->size});
    }
    auto saved_attributes =
        p_res_chunk.push_data(attributes.size() * sizeof(RUVertexData::Attribute), attributes.data()).force_cast<RUVertexData::Attribute>();

    RUVertexData vertex_data;
    vertex_data.vertices = saved_vertices;
    vertex_data.vertices_size = vertices.size();
    vertex_data.attributes = saved_attributes;
    vertex_data.attribute_count = attributes.size();
    vertex_data.vertex_stride = p_sections.vertex_stride;
    vertex_data.vertex_count = p_sections.vertex_count;
    return p_res_chunk.push_res_unit("", vertex_data).force_cast<RUVertexData>();
}

std::vector<uint32_t> build_indices_from_geometry_sections(
    const std::vector<uint8_t>& p_geometry_data, const RUSchemaSubmesh3DGeometrySection& p_index_section, size_t p_vertex_count) {
    std::vector<uint32_t> indices(get_geometry_element_count(p_index_section, sizeof(uint32_t)));
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

void validate_material_texture_role(std::string_view p_texture_role) {
    if (p_texture_role.empty() || p_texture_role.contains('\0')) {
        throw std::runtime_error("Failed to populate material resource: texture_role must be non-empty and must not contain NUL.");
    }
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
    for (const RUSchemaMaterialTextureSlot& texture_scheme : p_material.textures) {
        validate_material_texture_role(texture_scheme.texture_role);
        const HashCode texture_role_hash = dynam_hash(texture_scheme.texture_role);
        if (std::ranges::any_of(texture_slots, [&texture_scheme](const RUMaterial::TextureSlot& p_texture_slot) {
                return p_texture_slot.role.get_name() == texture_scheme.texture_role;
            })) {
            throw std::runtime_error(std::format("Failed to populate material resource: texture_role '{}' is declared more than once.",
                texture_scheme.texture_role));
        }
        if (std::ranges::any_of(texture_slots, [texture_role_hash](const RUMaterial::TextureSlot& p_texture_slot) {
                return p_texture_slot.role.hash == texture_role_hash;
            })) {
            throw std::runtime_error(std::format("Failed to populate material resource: texture_role '{}' collides with another role at hash {}.",
                texture_scheme.texture_role,
                texture_role_hash));
        }
        if (texture_scheme.texture.has_value() == texture_scheme.texture_id.has_value()) {
            throw std::runtime_error("Failed to populate material resource: texture requires exactly one of texture or texture_id.");
        }
        RUMaterial::TextureSlot texture_slot;
        texture_slot.role = populate_shader_role(p_res_chunk, texture_scheme.texture_role);
        if (texture_scheme.texture_id.has_value()) {
            texture_ids.push_back(*texture_scheme.texture_id);
        } else {
            texture_slot.texture = populate_image_resource(p_res_chunk, *texture_scheme.texture);
            texture_ids.emplace_back();
        }
        texture_slots.push_back(texture_slot);
    }
    resource_unit.texture_count = texture_slots.size();
    if (!texture_slots.empty()) {
        resource_unit.textures =
            p_res_chunk.push_data(texture_slots.size() * sizeof(RUMaterial::TextureSlot), texture_slots.data()).force_cast<RUMaterial::TextureSlot>();
    }

    auto saved_resource = p_res_chunk.push_res_unit(p_material.id, resource_unit).force_cast<RUMaterial>();
    if (!p_material.graphics_pipeline_ids.empty()) {
        p_res_chunk.create_ref_to(saved_resource->graphics_pipeline, p_material.graphics_pipeline_ids.front());
    }
    RUMaterial::TextureSlot* saved_texture_slots = saved_resource->textures.get();
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
    auto indices = build_indices_from_geometry_sections(geometry_data, *sections_by_slot.index_section, sections_by_slot.vertex_count);
    p_resource_unit.vertex_data = populate_vertex_data(p_res_chunk, geometry_data, sections_by_slot);
    p_resource_unit.index_count = indices.size();
    if (!indices.empty()) {
        p_resource_unit.indices = p_res_chunk.push_data(indices.size() * sizeof(uint32_t), indices.data()).force_cast<uint32_t>();
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
