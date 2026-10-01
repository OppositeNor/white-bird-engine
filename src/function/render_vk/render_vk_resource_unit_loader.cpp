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

const GeometryFormatInfo* get_geometry_format_info(std::string_view p_type) {
    for (const auto& format_info : GEOMETRY_ATTRIBUTE_FORMAT_INFOS) {
        if (format_info.type == p_type) {
            return &format_info;
        }
    }
    return nullptr;
}

void validate_binary_view_schema(const RUSchemaBinaryView& p_view, std::string_view p_context) {
    if (p_view.binary_id.empty() || p_view.binary_id.contains('\0')) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: {} binary_id is invalid.", p_context));
    }
    if (p_view.start % sizeof(uint32_t) != 0) {
        throw std::runtime_error(std::format(
            "Failed to populate submesh resource unit: {} starts at unaligned byte offset {}.", p_context, p_view.start));
    }
}

void validate_vertex_view_schema(const RUSchemaVertexView& p_vertices) {
    validate_binary_view_schema(p_vertices.binary, "vertex view");
    if (p_vertices.stride == 0) {
        throw std::runtime_error("Failed to populate submesh resource unit: vertex stride is zero.");
    }
    if (p_vertices.binary.size % p_vertices.stride != 0) {
        throw std::runtime_error("Failed to populate submesh resource unit: vertex view byte size is not a multiple of stride.");
    }
    if (p_vertices.attributes.empty()) {
        throw std::runtime_error("Failed to populate submesh resource unit: at least one vertex attribute is required.");
    }
    for (size_t i = 0; i < p_vertices.attributes.size(); ++i) {
        const RUSchemaVertexAttribute& attribute = p_vertices.attributes[i];
        if (attribute.role.empty() || attribute.role.contains('\0')) {
            throw std::runtime_error(
                "Failed to populate submesh resource unit: vertex attribute role must be non-empty and must not contain NUL.");
        }
        const GeometryFormatInfo* format_info = get_geometry_format_info(attribute.type);
        if (format_info == nullptr) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: unsupported vertex attribute type '{}' for role '{}'.",
                attribute.type,
                attribute.role));
        }
        if (attribute.offset > p_vertices.stride || format_info->size > p_vertices.stride - attribute.offset) {
            throw std::runtime_error(std::format(
                "Failed to populate submesh resource unit: vertex attribute '{}' exceeds the vertex stride.", attribute.role));
        }
        if (std::ranges::any_of(p_vertices.attributes.begin(),
                p_vertices.attributes.begin() + static_cast<std::ptrdiff_t>(i),
                [&attribute](const RUSchemaVertexAttribute& p_existing) {
                    return p_existing.role == attribute.role;
                })) {
            throw std::runtime_error(
                std::format("Failed to populate submesh resource unit: duplicate vertex attribute '{}'.", attribute.role));
        }
        if (std::ranges::any_of(p_vertices.attributes.begin(),
                p_vertices.attributes.begin() + static_cast<std::ptrdiff_t>(i),
                [&attribute](const RUSchemaVertexAttribute& p_existing) {
                    return dynam_hash(p_existing.role) == dynam_hash(attribute.role);
                })) {
            throw std::runtime_error(std::format("Failed to populate submesh resource unit: distinct geometry section roles collide at hash {}.",
                dynam_hash(attribute.role)));
        }
    }
}

void validate_index_view_schema(const RUSchemaIndexView& p_indices) {
    validate_binary_view_schema(p_indices.binary, "index view");
    if (p_indices.binary.size % sizeof(uint32_t) != 0) {
        throw std::runtime_error("Failed to populate submesh resource unit: index view byte size is not a multiple of uint32.");
    }
    if ((p_indices.binary.size / sizeof(uint32_t)) % 3 != 0) {
        throw std::runtime_error("Failed to populate submesh resource unit: index count is not a multiple of 3.");
    }
}

RUShaderRole populate_shader_role(IResourceChunk& p_res_chunk, std::string_view p_role_name) {
    if (p_role_name.empty() || p_role_name.contains('\0')) {
        throw std::runtime_error("Failed to populate shader role: role name must be non-empty and must not contain NUL.");
    }
    const std::string retained_name(p_role_name);
    return RUShaderRole(p_res_chunk.push_data(retained_name.size() + 1, retained_name.c_str()), dynam_hash(p_role_name));
}

void populate_binary_view(const RUSchemaBinaryView& p_scheme, RUBinaryView& p_binary_view) {
    p_binary_view.start = p_scheme.start;
    p_binary_view.size = p_scheme.size;
}

void populate_vertex_view(IResourceChunk& p_res_chunk, const RUSchemaVertexView& p_scheme, RUVertexView& p_vertex_view) {
    validate_vertex_view_schema(p_scheme);
    populate_binary_view(p_scheme.binary, p_vertex_view.binary);

    std::vector<RUVertexView::Attribute> attributes;
    attributes.reserve(p_scheme.attributes.size());
    for (const RUSchemaVertexAttribute& attribute_scheme : p_scheme.attributes) {
        const GeometryFormatInfo* format_info = get_geometry_format_info(attribute_scheme.type);
        WBE_DEBUG_ASSERT(format_info != nullptr);
        attributes.push_back(RUVertexView::Attribute{.role = populate_shader_role(p_res_chunk, attribute_scheme.role),
            .format = format_info->format,
            .offset = attribute_scheme.offset,
            .size = format_info->size});
    }
    p_vertex_view.attributes =
        p_res_chunk.push_data(attributes.size() * sizeof(RUVertexView::Attribute), attributes.data()).force_cast<RUVertexView::Attribute>();
    p_vertex_view.attribute_count = attributes.size();
    p_vertex_view.vertex_stride = p_scheme.stride;
    p_vertex_view.vertex_count = p_scheme.binary.size / p_scheme.stride;
}

void populate_index_view(const RUSchemaIndexView& p_scheme, RUIndexView& p_index_view) {
    validate_index_view_schema(p_scheme);
    populate_binary_view(p_scheme.binary, p_index_view.binary);
    p_index_view.index_count = p_scheme.binary.size / sizeof(uint32_t);
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

RefRes<RUTexture> populate_texture_resource(IResourceChunk& p_res_chunk, const RUSchemaTexture& p_scheme) {
    if (p_scheme.path.empty()) {
        throw std::runtime_error("Failed to populate texture resource: inline texture requires path.");
    }
    ResourceUnitLoaderTexture texture_loader;
    return std::any_cast<RefRes<RUTexture>>(texture_loader.populate_scheme_ptr(p_res_chunk, p_scheme));
}

RefRes<RUShader> populate_shader_resource(IResourceChunk& p_res_chunk, const RUSchemaShader& p_scheme) {
    auto file_path = get_resource_path(p_scheme.path);
    auto file_content = load_binary_file<uint32_t>(file_path.string().c_str());
    auto data = p_res_chunk.push_data(file_content.size() * sizeof(decltype(*file_content.data())), file_content.data());
    RUShader resource_unit;
    resource_unit.metadata_path = populate_text(p_res_chunk, p_scheme.metadata);
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
    resource_unit.conservative_rasterization = p_scheme.conservative_rasterization;
    auto saved_resource = p_res_chunk.push_res_unit(p_scheme.id, resource_unit).force_cast<RUGraphicsPipeline>();
    create_pipeline_stage_ref(p_res_chunk, saved_resource->vertex_stage, p_scheme.vertex_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->tessellation_stage, p_scheme.tessellation_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->geometry_stage, p_scheme.geometry_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->fragment_stage, p_scheme.fragment_stage_id);
    return saved_resource;
}

RefRes<RURayTracingPipeline> populate_ray_tracing_pipeline_resource(IResourceChunk& p_res_chunk, const RUSchemaRayTracingPipeline& p_scheme) {
    RURayTracingPipeline resource_unit;
    populate_pipeline_stage(p_res_chunk, resource_unit.raygen_stage, p_scheme.raygen_stage_id, p_scheme.raygen_stage, "raygen", false);
    populate_pipeline_stage(p_res_chunk, resource_unit.miss_stage, p_scheme.miss_stage_id, p_scheme.miss_stage, "miss", false);
    populate_pipeline_stage(
        p_res_chunk, resource_unit.closest_hit_stage, p_scheme.closest_hit_stage_id, p_scheme.closest_hit_stage, "closest_hit", false);
    populate_pipeline_stage(p_res_chunk, resource_unit.any_hit_stage, p_scheme.any_hit_stage_id, p_scheme.any_hit_stage, "any_hit", false);
    populate_pipeline_stage(p_res_chunk,
        resource_unit.intersection_shader_stage,
        p_scheme.intersection_shader_stage_id,
        p_scheme.intersection_shader_stage,
        "intersection_shader",
        false);
    resource_unit.raygen_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(
            p_scheme.raygen_stage_entry, scheme_has_pipeline_stage(p_scheme.raygen_stage_id, p_scheme.raygen_stage)));
    resource_unit.miss_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(p_scheme.miss_stage_entry, scheme_has_pipeline_stage(p_scheme.miss_stage_id, p_scheme.miss_stage)));
    resource_unit.closest_hit_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(p_scheme.closest_hit_stage_entry,
            scheme_has_pipeline_stage(p_scheme.closest_hit_stage_id, p_scheme.closest_hit_stage)));
    resource_unit.any_hit_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(
            p_scheme.any_hit_stage_entry, scheme_has_pipeline_stage(p_scheme.any_hit_stage_id, p_scheme.any_hit_stage)));
    resource_unit.intersection_shader_stage_entry = push_stage_entry(p_res_chunk,
        resolve_pipeline_stage_entry(p_scheme.intersection_shader_stage_entry,
            scheme_has_pipeline_stage(p_scheme.intersection_shader_stage_id, p_scheme.intersection_shader_stage)));
    auto saved_resource = p_res_chunk.push_res_unit(p_scheme.id, resource_unit).force_cast<RURayTracingPipeline>();
    create_pipeline_stage_ref(p_res_chunk, saved_resource->raygen_stage, p_scheme.raygen_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->miss_stage, p_scheme.miss_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->closest_hit_stage, p_scheme.closest_hit_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->any_hit_stage, p_scheme.any_hit_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->intersection_shader_stage, p_scheme.intersection_shader_stage_id);
    return saved_resource;
}

void validate_material_texture_role(std::string_view p_texture_role) {
    if (p_texture_role.empty() || p_texture_role.contains('\0')) {
        throw std::runtime_error("Failed to populate material resource: texture_role must be non-empty and must not contain NUL.");
    }
}

RefRes<RUMaterial> resolve_material(IResourceChunk& p_res_chunk, const RUSchemaMaterial& p_material) { // NOLINT
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
            texture_slot.texture = populate_texture_resource(p_res_chunk, *texture_scheme.texture);
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

    populate_vertex_view(p_res_chunk, p_scheme.vertices, p_resource_unit.vertices);
    populate_index_view(p_scheme.indices, p_resource_unit.indices);
}

void create_submesh_material_ref(IResourceChunk& p_res_chunk, RUSubmesh3D& p_resource_unit, const RUSchemaSubmesh3D& p_scheme) {
    if (p_scheme.material_id.has_value()) {
        p_res_chunk.create_ref_to(p_resource_unit.material, p_scheme.material_id.value());
    }
    p_res_chunk.create_ref_to(p_resource_unit.vertices.binary.binary, p_scheme.vertices.binary.binary_id);
    p_res_chunk.create_ref_to(p_resource_unit.indices.binary.binary, p_scheme.indices.binary.binary_id);
}

void resolve_static_geometry_submesh_data(
    IResourceChunk& p_res_chunk, const RUSchemaStaticGeometrySubmesh& p_scheme, RUStaticGeometrySubmesh& p_resource_unit) {
    if (!p_scheme.material_id.has_value()) {
        if (!p_scheme.material.has_value()) {
            throw std::runtime_error("Failed to populate static geometry set: material_id or inline material is required.");
        }
        p_resource_unit.material = resolve_material(p_res_chunk, p_scheme.material.value());
    }
    populate_vertex_view(p_res_chunk, p_scheme.vertices, p_resource_unit.vertices);
    populate_index_view(p_scheme.indices, p_resource_unit.indices);
    p_resource_unit.first_instance = p_scheme.first_instance;
    p_resource_unit.instance_count = p_scheme.instance_count;
}

void create_static_geometry_submesh_refs(
    IResourceChunk& p_res_chunk, RUStaticGeometrySubmesh& p_resource_unit, const RUSchemaStaticGeometrySubmesh& p_scheme) {
    if (p_scheme.material_id.has_value()) {
        p_res_chunk.create_ref_to(p_resource_unit.material, p_scheme.material_id.value());
    }
    p_res_chunk.create_ref_to(p_resource_unit.vertices.binary.binary, p_scheme.vertices.binary.binary_id);
    p_res_chunk.create_ref_to(p_resource_unit.indices.binary.binary, p_scheme.indices.binary.binary_id);
}

RUStaticGeometryInstance populate_static_geometry_instance(const RUSchemaStaticGeometryInstance& p_scheme) {
    return RUStaticGeometryInstance{.global_transform = p_scheme.global_transform};
}

template <typename RuntimeType, typename SchemaType>
RefRes<RuntimeType> populate_static_geometry_set(IResourceChunk& p_res_chunk, const SchemaType& p_scheme) {
    RuntimeType resource_unit;

    std::vector<RUStaticGeometrySubmesh> submeshes;
    submeshes.reserve(p_scheme.submeshes.size());
    for (const RUSchemaStaticGeometrySubmesh& submesh_scheme : p_scheme.submeshes) {
        if (submesh_scheme.first_instance > p_scheme.instances.size() ||
            submesh_scheme.instance_count > p_scheme.instances.size() - submesh_scheme.first_instance) {
            throw std::runtime_error("Failed to populate static geometry set: submesh instance range is out of bounds.");
        }
        RUStaticGeometrySubmesh submesh;
        resolve_static_geometry_submesh_data(p_res_chunk, submesh_scheme, submesh);
        submeshes.push_back(submesh);
    }
    if (!submeshes.empty()) {
        resource_unit.submeshes =
            p_res_chunk.push_data(submeshes.size() * sizeof(RUStaticGeometrySubmesh), submeshes.data()).force_cast<RUStaticGeometrySubmesh>();
    }
    resource_unit.submesh_count = submeshes.size();

    std::vector<RUStaticGeometryInstance> instances;
    instances.reserve(p_scheme.instances.size());
    for (const RUSchemaStaticGeometryInstance& instance_scheme : p_scheme.instances) {
        instances.push_back(populate_static_geometry_instance(instance_scheme));
    }
    if (!instances.empty()) {
        resource_unit.instances =
            p_res_chunk.push_data(instances.size() * sizeof(RUStaticGeometryInstance), instances.data()).force_cast<RUStaticGeometryInstance>();
    }
    resource_unit.instance_count = instances.size();

    auto saved_resource = p_res_chunk.push_res_unit(p_scheme.id, resource_unit).template force_cast<RuntimeType>();
    for (size_t i = 0; i < p_scheme.submeshes.size(); ++i) {
        create_static_geometry_submesh_refs(p_res_chunk, saved_resource->submeshes.get()[i], p_scheme.submeshes[i]);
    }
    return saved_resource;
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

std::span<const std::string> ResourceUnitLoaderRayTracingPipeline::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"ray_tracing_pipeline"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderRayTracingPipeline::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaRayTracingPipeline>(p_scheme, "ray_tracing_pipeline");
    return populate_ray_tracing_pipeline_resource(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderRayTracingPipeline::populate_scheme_from_data(
    IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaRayTracingPipeline scheme;
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

std::span<const std::string> ResourceUnitLoaderStaticOpaqueSet::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"static_opaque_set"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderStaticOpaqueSet::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaStaticOpaqueSet>(p_scheme, "static_opaque_set");
    return populate_static_geometry_set<RUStaticOpaqueSet>(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderStaticOpaqueSet::populate_scheme_from_data(
    IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaStaticOpaqueSet scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderStaticOpaqueDoubleSidedSet::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"static_opaque_double_sided_set"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderStaticOpaqueDoubleSidedSet::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaStaticOpaqueDoubleSidedSet>(p_scheme, "static_opaque_double_sided_set");
    return populate_static_geometry_set<RUStaticOpaqueDoubleSidedSet>(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderStaticOpaqueDoubleSidedSet::populate_scheme_from_data(
    IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaStaticOpaqueDoubleSidedSet scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderStaticMaskedSet::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"static_masked_set"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderStaticMaskedSet::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaStaticMaskedSet>(p_scheme, "static_masked_set");
    return populate_static_geometry_set<RUStaticMaskedSet>(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderStaticMaskedSet::populate_scheme_from_data(
    IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaStaticMaskedSet scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

void register_renderer_vk_resource_unit_loaders() {
    auto* resource_manager = ResourceManager::get_singleton();
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderShader>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderGraphicsPipeline>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderRayTracingPipeline>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderMaterial>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderMesh>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderSubmesh>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderStaticOpaqueSet>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderStaticOpaqueDoubleSidedSet>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderStaticMaskedSet>(global_allocator()));
}

} // namespace WhiteBirdEngine
