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
#include "function/render_vk/render_vk_resource_unit.hh"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace WhiteBirdEngine {
namespace {

template <typename SchemeType>
const SchemeType& get_typed_scheme(const RUScheme& p_scheme, const std::string& p_expected_type) {
    const auto* typed_scheme = dynamic_cast<const SchemeType*>(&p_scheme);
    if (typed_scheme == nullptr) {
        throw std::runtime_error(std::format("Failed to populate resource: expected typed scheme {} "
                                             "but got type {}.",
            p_expected_type,
            p_scheme.type));
    }
    return *typed_scheme;
}

RefRes<char> populate_text(IResourceChunk& p_res_chunk, const std::string& p_str) {
    return p_res_chunk.push_data(p_str.size() + 1, p_str.c_str());
}

RefRes<RUText> populate_text_resource(IResourceChunk& p_res_chunk, const std::string& p_str) {
    RUText resource_unit;
    resource_unit.text_data = populate_text(p_res_chunk, p_str);
    resource_unit.size = p_str.size() + 1;
    return p_res_chunk.push_res_unit("", resource_unit).reint_cast<RUText>();
}

void resolve_image_data(IResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) {
    auto file_path = get_resource_path(p_scheme.path);
    uint32_t width, height, file_channels;
    auto image_data = load_image_data(file_path, p_scheme.channel_count, p_scheme.flip_h, p_scheme.flip_v, width, height, file_channels);
    if (image_data == nullptr) {
        throw std::runtime_error(std::format("Failed to load image data from file: {}", static_cast<std::string>(file_path)));
    }
    p_resource_unit.width = width;
    p_resource_unit.height = height;
    p_resource_unit.channel_count = p_scheme.channel_count == 0 ? file_channels : p_scheme.channel_count;
    p_resource_unit.color_space = get_color_space_from_str(p_scheme.color_space);
    auto data = p_res_chunk.push_data(width * height * p_resource_unit.channel_count, image_data.get());
    p_resource_unit.image_data = data;
}

void resolve_hdr_image_data(IResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) {
    auto file_path = get_resource_path(p_scheme.path);
    uint32_t width, height, file_channels;
    auto image_data =
        load_image_data_hdr(file_path, p_scheme.channel_count, p_scheme.flip_h, p_scheme.flip_v, width, height, file_channels);
    if (image_data == nullptr) {
        throw std::runtime_error(std::format("Failed to load HDR image data from file: {}", static_cast<std::string>(file_path)));
    }
    p_resource_unit.width = width;
    p_resource_unit.height = height;
    p_resource_unit.channel_count = p_scheme.channel_count == 0 ? file_channels : p_scheme.channel_count;
    p_resource_unit.color_space = get_color_space_from_str(p_scheme.color_space);
    p_resource_unit.config.set(RUImage::IS_HDR);
    auto data = p_res_chunk.push_data(width * height * p_resource_unit.channel_count * sizeof(float), image_data.get());
    p_resource_unit.image_data = data;
}

RefRes<RUImage> populate_image_resource(IResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme) {
    if (p_scheme.path.empty()) {
        throw std::runtime_error("Failed to populate image resource: inline image requires path.");
    }
    RUImage resource_unit;
    if (p_scheme.is_hdr) {
        resolve_hdr_image_data(p_res_chunk, p_scheme, resource_unit);
    } else {
        resolve_image_data(p_res_chunk, p_scheme, resource_unit);
    }
    return p_res_chunk.push_res_unit(p_scheme.id, resource_unit).reint_cast<RUImage>();
}

RefRes<RUShader> populate_shader_resource(IResourceChunk& p_res_chunk, const RUSchemeShader& p_scheme) {
    auto file_path = get_resource_path(p_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    RUShader resource_unit;
    resource_unit.metadata_path = populate_text(p_res_chunk, p_scheme.metadata);
    resource_unit.data = data;
    resource_unit.size = file_content.size();
    return p_res_chunk.push_res_unit(p_scheme.id, resource_unit).reint_cast<RUShader>();
}

void populate_pipeline_stage(IResourceChunk& p_res_chunk,
    RefRes<RUShader>& p_stage,
    const std::string& p_stage_id,
    const RUSchemeShader& p_stage_scheme,
    const std::string& p_stage_name,
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

void create_pipeline_stage_ref(IResourceChunk& p_res_chunk, RefRes<RUShader>& p_stage, const std::string& p_stage_id) {
    if (!p_stage_id.empty()) {
        p_res_chunk.create_ref_to(p_stage, p_stage_id);
    }
}

RefRes<char> push_stage_entry(IResourceChunk& p_res_chunk, const std::string& p_entry) {
    const std::string& stage_entry = p_entry.empty() ? std::string("main") : p_entry;
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
    auto saved_resource = p_res_chunk.push_res_unit(p_scheme.id, resource_unit).reint_cast<RUGraphicsPipeline>();
    create_pipeline_stage_ref(p_res_chunk, saved_resource->vertex_stage, p_scheme.vertex_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->tessellation_stage, p_scheme.tessellation_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->geometry_stage, p_scheme.geometry_stage_id);
    create_pipeline_stage_ref(p_res_chunk, saved_resource->fragment_stage, p_scheme.fragment_stage_id);
    return saved_resource;
}

RUSchemeMaterialTextureSlot parse_material_texture_slot(const std::string& p_texture_key, const ParserJSON::DataType& p_texture_data) {
    RUSchemeMaterialTextureSlot texture_slot;
    texture_slot.texture_key = p_texture_key;
    const std::string resource_type = p_texture_data.get_value<std::string>("type");
    if (resource_type == "resource_reference") {
        texture_slot.texture_id = p_texture_data.get_value<std::string>("id");
        return texture_slot;
    }
    if (resource_type == "image") {
        texture_slot.texture.deserialize(p_texture_data);
        return texture_slot;
    }
    throw std::runtime_error(
        std::format("Failed to populate material texture '{}': unsupported resource type '{}'.", p_texture_key, resource_type));
}

struct ParsedMaterialScheme {
    RUSchemeMaterial material;
    RUSchemeGraphicsPipeline graphics_pipeline;
    bool has_graphics_pipeline = false;
};

void parse_material_graphics_pipeline(ParsedMaterialScheme& p_scheme, const ParserJSON::DataType& p_graphics_pipeline_data) {
    const std::string resource_type = p_graphics_pipeline_data.get_value<std::string>("type");
    if (resource_type == "resource_reference") {
        if (p_scheme.has_graphics_pipeline || !p_scheme.material.graphics_pipeline_id.empty()) {
            throw std::runtime_error("Failed to populate material resource: graphics pipeline is declared more than once.");
        }
        p_scheme.material.graphics_pipeline_id = p_graphics_pipeline_data.get_value<std::string>("id");
        return;
    }
    if (resource_type == "graphics_pipeline") {
        if (p_scheme.has_graphics_pipeline || !p_scheme.material.graphics_pipeline_id.empty()) {
            throw std::runtime_error("Failed to populate material resource: graphics pipeline is declared more than once.");
        }
        p_scheme.graphics_pipeline.deserialize(p_graphics_pipeline_data);
        p_scheme.has_graphics_pipeline = true;
        return;
    }
    throw std::runtime_error(
        std::format("Failed to populate material resource: unsupported graphics pipeline resource type '{}'.", resource_type));
}

ParsedMaterialScheme parse_material_scheme(const ParserJSON::DataType& p_scheme_data) {
    ParsedMaterialScheme scheme;
    if (p_scheme_data.contains("id")) {
        scheme.material.id = p_scheme_data.get_value<std::string>("id");
    }
    scheme.material.type = p_scheme_data.get_value<std::string>("type");
    if (p_scheme_data.contains("graphics_pipeline_id")) {
        scheme.material.graphics_pipeline_id = p_scheme_data.get_value<std::string>("graphics_pipeline_id");
    }
    if (p_scheme_data.contains("graphics_pipeline")) {
        parse_material_graphics_pipeline(scheme, p_scheme_data.get_value<ParserJSON::DataType>("graphics_pipeline"));
    }
    if (p_scheme_data.contains("shading_model")) {
        parse_material_graphics_pipeline(scheme, p_scheme_data.get_value<ParserJSON::DataType>("shading_model"));
    }

    if (p_scheme_data.contains("textures")) {
        auto textures_data = p_scheme_data.get_value<ParserJSON::DataType>("textures");
        for (const auto& texture_key : textures_data.get_all_keys()) {
            scheme.material.textures.push_back(
                parse_material_texture_slot(texture_key, textures_data.get_value<ParserJSON::DataType>(texture_key)));
        }
    }
    return scheme;
}

RefRes<RUMaterial> resolve_material(
    IResourceChunk& p_res_chunk, const RUSchemeMaterial& p_material, const RUSchemeGraphicsPipeline* p_graphics_pipeline = nullptr) {
    if (p_material.graphics_pipeline_id.empty() && p_graphics_pipeline == nullptr) {
        throw std::runtime_error("Failed to populate material resource: graphics_pipeline_id or inline graphics_pipeline is required.");
    }

    RUMaterial resource_unit;
    if (p_graphics_pipeline != nullptr) {
        resource_unit.graphics_pipeline = populate_graphics_pipeline_resource(p_res_chunk, *p_graphics_pipeline);
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
        resource_unit.textures = texture_slot_data.reint_cast<RUMaterial::TextureSlot>();
    }

    auto saved_resource = p_res_chunk.push_res_unit(p_material.id, resource_unit).reint_cast<RUMaterial>();
    if (!p_material.graphics_pipeline_id.empty()) {
        p_res_chunk.create_ref_to(saved_resource->graphics_pipeline, p_material.graphics_pipeline_id);
    }
    auto* saved_texture_slots = saved_resource->textures.get();
    for (size_t i = 0; i < texture_ids.size(); ++i) {
        if (!texture_ids[i].empty()) {
            p_res_chunk.create_ref_to(saved_texture_slots[i].texture, texture_ids[i]);
        }
    }
    return saved_resource;
}

template <typename LoaderType, typename SchemeType>
void populate_from_parser_data(const LoaderType& p_loader, IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) {
    SchemeType scheme;
    scheme.deserialize(p_scheme_data);
    p_loader.populate_scheme(p_res_chunk, scheme);
}

} // namespace

std::vector<std::string> ResourceUnitLoaderShader::get_supported_resource_types() const {
    return {"shader"};
}

void ResourceUnitLoaderShader::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeShader>(p_scheme, "shader");
    populate_shader_resource(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderShader::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderShader, RUSchemeShader>(*this, p_res_chunk, p_scheme_data);
}

std::vector<std::string> ResourceUnitLoaderGraphicsPipeline::get_supported_resource_types() const {
    return {"graphics_pipeline"};
}

void ResourceUnitLoaderGraphicsPipeline::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeGraphicsPipeline>(p_scheme, "graphics_pipeline");
    populate_graphics_pipeline_resource(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderGraphicsPipeline::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderGraphicsPipeline, RUSchemeGraphicsPipeline>(*this, p_res_chunk, p_scheme_data);
}

std::vector<std::string> ResourceUnitLoaderMaterial::get_supported_resource_types() const {
    return {"material"};
}

void ResourceUnitLoaderMaterial::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeMaterial>(p_scheme, "material");
    resolve_material(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderMaterial::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    auto scheme = parse_material_scheme(p_scheme_data);
    resolve_material(p_res_chunk, scheme.material, scheme.has_graphics_pipeline ? &scheme.graphics_pipeline : nullptr);
}

void register_renderer_vk_resource_unit_loaders() {
    auto* resource_manager = ResourceManager::get_singleton();
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderShader>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderGraphicsPipeline>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderMaterial>(global_allocator()));
}

} // namespace WhiteBirdEngine
