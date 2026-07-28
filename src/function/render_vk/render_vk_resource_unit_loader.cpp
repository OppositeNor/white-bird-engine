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
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "resource/resource_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <any>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {
namespace {

template <typename SchemeType>
const SchemeType& get_typed_scheme(const RUScheme& p_scheme, std::string_view p_expected_type) {
    const auto* typed_scheme = dynamic_cast<const SchemeType*>(&p_scheme);
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

RefRes<RUImage> populate_image_resource(IResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme) {
    if (p_scheme.path.empty()) {
        throw std::runtime_error("Failed to populate image resource: inline image requires path.");
    }
    ResourceUnitLoaderImage image_loader;
    return std::any_cast<RefRes<RUImage>>(
        static_cast<const IResourceUnitLoader&>(image_loader).populate_scheme_ptr(p_res_chunk, p_scheme));
}

RefRes<RUShader> populate_shader_resource(IResourceChunk& p_res_chunk, const RUSchemeShader& p_scheme) {
    auto file_path = get_resource_path(p_scheme.path);
    auto file_content = load_binary_file<uint32_t>(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size() * sizeof(decltype(*file_content.data())), file_content.data());
    RUShader resource_unit;
    resource_unit.metadata_path = populate_text(p_res_chunk, p_scheme.metadata);
    resource_unit.data = data.force_cast<uint32_t>();
    resource_unit.size = file_content.size();
    return p_res_chunk.push_res_unit(p_scheme.id, resource_unit).force_cast<RUShader>();
}

void populate_pipeline_stage(IResourceChunk& p_res_chunk,
    RefRes<RUShader>& p_stage,
    std::string_view p_stage_id,
    const RUSchemeShader& p_stage_scheme,
    std::string_view p_stage_name,
    bool p_is_required = true) {
    if (!p_stage_id.empty()) {
        return;
    }
    if (p_stage_scheme.path.empty()) {
        if (!p_is_required) {
            return;
        }
        throw std::runtime_error(std::format("Failed to populate pipeline resource: {} stage "
                                             "requires either stage id or inline shader path.",
            p_stage_name));
    }
    p_stage = populate_shader_resource(p_res_chunk, p_stage_scheme);
}

void create_pipeline_stage_ref(IResourceChunk& p_res_chunk, RefRes<RUShader>& p_stage, std::string_view p_stage_id) {
    if (!p_stage_id.empty()) {
        p_res_chunk.create_ref_to(p_stage, p_stage_id);
    }
}

RefRes<char> push_stage_entry(IResourceChunk& p_res_chunk, std::string_view p_entry) {
    std::string stage_entry(p_entry.empty() ? "main" : p_entry);
    return p_res_chunk.push_data(stage_entry.size() + 1, stage_entry.c_str());
}

RefRes<RUGraphicsPipeline> populate_graphics_pipeline_resource(IResourceChunk& p_res_chunk, const RUSchemeGraphicsPipeline& p_scheme) {
    RUGraphicsPipeline resource_unit;
    populate_pipeline_stage(p_res_chunk, resource_unit.vertex_stage, p_scheme.vertex_stage_id, p_scheme.vertex_stage, "vertex");
    populate_pipeline_stage(
        p_res_chunk, resource_unit.tessellation_stage, p_scheme.tessellation_stage_id, p_scheme.tessellation_stage, "tessellation", false);
    populate_pipeline_stage(
        p_res_chunk, resource_unit.geometry_stage, p_scheme.geometry_stage_id, p_scheme.geometry_stage, "geometry", false);
    populate_pipeline_stage(p_res_chunk, resource_unit.fragment_stage, p_scheme.fragment_stage_id, p_scheme.fragment_stage, "fragment");
    resource_unit.vertex_stage_entry = push_stage_entry(p_res_chunk, p_scheme.vertex_stage_entry);
    resource_unit.tessellation_stage_entry = push_stage_entry(p_res_chunk, p_scheme.tessellation_stage_entry);
    resource_unit.geometry_stage_entry = push_stage_entry(p_res_chunk, p_scheme.geometry_stage_entry);
    resource_unit.fragment_stage_entry = push_stage_entry(p_res_chunk, p_scheme.fragment_stage_entry);
    resource_unit.depth_test = p_scheme.depth_test;
    resource_unit.depth_write = p_scheme.depth_write;
    auto saved_resource = p_res_chunk.push_res_unit(p_scheme.id, resource_unit).force_cast<RUGraphicsPipeline>();
    create_pipeline_stage_ref(p_res_chunk, saved_resource->vertex_stage, p_scheme.vertex_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->tessellation_stage, p_scheme.tessellation_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->geometry_stage, p_scheme.geometry_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->fragment_stage, p_scheme.fragment_stage_id);
    return saved_resource;
}

RefRes<RUMaterial> resolve_material(IResourceChunk& p_res_chunk, const RUSchemeMaterial& p_material) {
    const bool has_inline_pipeline = !p_material.graphics_pipeline.type.empty();
    if (p_material.graphics_pipeline_ids.empty() && !has_inline_pipeline) {
        throw std::runtime_error("Failed to populate material resource: graphics_pipeline_ids or inline graphics_pipeline is required.");
    }
    if (!p_material.graphics_pipeline_ids.empty() && has_inline_pipeline) {
        throw std::runtime_error("Failed to populate material resource: graphics pipeline is declared more than once.");
    }

    RUMaterial resource_unit;
    if (has_inline_pipeline) {
        resource_unit.graphics_pipeline = populate_graphics_pipeline_resource(p_res_chunk, p_material.graphics_pipeline);
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
        if (!texture_scheme.texture_id.empty()) {
            texture_ids.push_back(texture_scheme.texture_id);
        } else {
            texture_slot.texture = populate_image_resource(p_res_chunk, texture_scheme.texture);
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

void resolve_submesh_data(IResourceChunk& p_res_chunk, const RUSchemeSubmesh3D& p_scheme, RUSubmesh3D& p_resource_unit) {
    if (p_scheme.material_id.empty()) {
        p_resource_unit.material = resolve_material(p_res_chunk, p_scheme.material);
    }
    auto vertices_data =
        p_res_chunk.push_data(p_scheme.vertices_data.size() * sizeof(Vertex3DTextured), p_scheme.vertices_data.data());

    std::vector<Triangle3DIndxTextured> triangles(p_scheme.indices_data.size() / 3);
    for (size_t i = 0; i < triangles.size(); ++i) {
        auto& triangle = triangles.at(i);
        triangle.vert_array = vertices_data.force_cast<Vertex3DTextured>();
        triangle.vertex_indices[0] = p_scheme.indices_data.at(i * 3);
        triangle.vertex_indices[1] = p_scheme.indices_data.at(i * 3 + 1);
        triangle.vertex_indices[2] = p_scheme.indices_data.at(i * 3 + 2);
    }

    auto triangles_data = p_res_chunk.push_data(triangles.size() * sizeof(Triangle3DIndxTextured), triangles.data());
    p_resource_unit.triangles = triangles_data.force_cast<Triangle3DIndxTextured>();
    p_resource_unit.triangles_count = triangles.size();
}

void create_submesh_material_ref(IResourceChunk& p_res_chunk, RUSubmesh3D& p_resource_unit, const RUSchemeSubmesh3D& p_scheme) {
    if (!p_scheme.material_id.empty()) {
        p_res_chunk.create_ref_to(p_resource_unit.material, p_scheme.material_id);
    }
}

} // namespace

std::vector<std::string> ResourceUnitLoaderShader::get_supported_resource_types() const {
    return {"shader"};
}

std::any ResourceUnitLoaderShader::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeShader>(p_scheme, "shader");
    return populate_shader_resource(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderShader::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeShader scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderGraphicsPipeline::get_supported_resource_types() const {
    return {"graphics_pipeline"};
}

std::any ResourceUnitLoaderGraphicsPipeline::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeGraphicsPipeline>(p_scheme, "graphics_pipeline");
    return populate_graphics_pipeline_resource(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderGraphicsPipeline::populate_scheme_from_data(
    IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeGraphicsPipeline scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderMaterial::get_supported_resource_types() const {
    return {"material"};
}

std::any ResourceUnitLoaderMaterial::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeMaterial>(p_scheme, "material");
    return resolve_material(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderMaterial::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeMaterial scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderMesh::get_supported_resource_types() const {
    return {"mesh"};
}

std::any ResourceUnitLoaderMesh::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeMesh3D>(p_scheme, "mesh");
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
    RUSchemeMesh3D scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderSubmesh::get_supported_resource_types() const {
    return {"submesh"};
}

std::any ResourceUnitLoaderSubmesh::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeSubmesh3D>(p_scheme, "submesh");
    if (typed_scheme.indices_data.size() % 3 != 0) {
        throw std::runtime_error(std::format("Failed to populate submesh resource unit: indices "
                                             "data size {} is not a multiple of 3.",
            typed_scheme.indices_data.size()));
    }
    RUSubmesh3D resource_unit;
    resolve_submesh_data(p_res_chunk, typed_scheme, resource_unit);
    auto saved_resource = p_res_chunk.push_res_unit(typed_scheme.id, resource_unit).force_cast<RUSubmesh3D>();
    create_submesh_material_ref(p_res_chunk, *saved_resource, typed_scheme);
    return saved_resource;
}

void ResourceUnitLoaderSubmesh::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeSubmesh3D scheme;
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
