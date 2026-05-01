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
#include "function/renderer_vk/renderer_vk_resource_unit_loader.hh"
#include "core/parser/parser_json.hh"
#include "core/memory/reference_strong.hh"
#include "core/engine_core.hh"
#include "function/render_vk/render_vk_resource_unit.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "utils/utils.hh"
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
        throw std::runtime_error(std::format(
            "Failed to populate resource: expected typed scheme {} but got type {}.",
            p_expected_type,
            p_scheme.type));
    }
    return *typed_scheme;
}

RefRes<char> populate_text(IResourceChunk& p_res_chunk, const std::string& p_str) {
    return p_res_chunk.push_data(p_str.size() + 1, p_str.c_str());
}

RefRes<RUShader> populate_shader_resource(IResourceChunk& p_res_chunk, const RUSchemeShader& p_scheme) {
    auto file_path = get_resource_path(p_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    ParserJSON metadata_parser;
    metadata_parser.parse(get_resource_path(p_scheme.metadata));
    RUShader resource_unit;
    resource_unit.metadata_path = populate_text(p_res_chunk, p_scheme.metadata);
    resource_unit.metadata.deserialize(metadata_parser.get_data());
    resource_unit.data = data;
    resource_unit.size = file_content.size();
    return p_res_chunk.push_res_unit(p_scheme.id, resource_unit).reint_cast<RUShader>();
}

void resolve_pipeline_stage(
    IResourceChunk& p_res_chunk,
    RefRes<RUShader>& p_stage,
    const std::string& p_stage_id,
    const RUSchemeShader& p_stage_scheme,
    const std::string& p_stage_name,
    bool p_is_required = true) {
    if (!p_stage_id.empty()) {
        p_res_chunk.create_ref_to(reinterpret_cast<void**>(&p_stage), p_stage_id);
        return;
    }
    if (p_stage_scheme.path.empty()) {
        if (!p_is_required) {
            return;
        }
        throw std::runtime_error(std::format(
            "Failed to populate pipeline resource: {} stage requires either stage id or inline shader path.",
            p_stage_name));
    }
    p_stage = populate_shader_resource(p_res_chunk, p_stage_scheme);
}

RefRes<char> push_stage_entry(IResourceChunk& p_res_chunk, const std::string& p_entry) {
    const std::string& stage_entry = p_entry.empty() ? std::string("main") : p_entry;
    return p_res_chunk.push_data(stage_entry.size() + 1, stage_entry.c_str());
}

RefRes<RUMaterial> resolve_material(IResourceChunk& p_res_chunk, const RUSchemeMaterial& p_material) {
    (void)p_res_chunk;
    (void)p_material;
    // TODO
    auto ru_material = RUMaterial();
    return p_res_chunk.push_res_unit(p_material.id, ru_material).reint_cast<RUMaterial>();
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
    RUGraphicsPipeline resource_unit;
    resolve_pipeline_stage(
        p_res_chunk,
        resource_unit.vertex_stage,
        typed_scheme.vertex_stage_id,
        typed_scheme.vertex_stage,
        "vertex");
    resolve_pipeline_stage(
        p_res_chunk,
        resource_unit.tessellation_stage,
        typed_scheme.tessellation_stage_id,
        typed_scheme.tessellation_stage,
        "tessellation",
        false);
    resolve_pipeline_stage(
        p_res_chunk,
        resource_unit.geometry_stage,
        typed_scheme.geometry_stage_id,
        typed_scheme.geometry_stage,
        "geometry",
        false);
    resolve_pipeline_stage(
        p_res_chunk,
        resource_unit.fragment_stage,
        typed_scheme.fragment_stage_id,
        typed_scheme.fragment_stage,
        "fragment");
    resource_unit.vertex_stage_entry = push_stage_entry(p_res_chunk, typed_scheme.vertex_stage_entry);
    resource_unit.tessellation_stage_entry = push_stage_entry(p_res_chunk, typed_scheme.tessellation_stage_entry);
    resource_unit.geometry_stage_entry = push_stage_entry(p_res_chunk, typed_scheme.geometry_stage_entry);
    resource_unit.fragment_stage_entry = push_stage_entry(p_res_chunk, typed_scheme.fragment_stage_entry);
    resource_unit.depth_test = typed_scheme.depth_test;
    resource_unit.depth_write = typed_scheme.depth_write;
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
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
    populate_from_parser_data<ResourceUnitLoaderMaterial, RUSchemeMaterial>(*this, p_res_chunk, p_scheme_data);
}

void register_renderer_vk_resource_unit_loaders() {
    auto* resource_manager = ResourceManager::get_singleton();
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderShader>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderGraphicsPipeline>(global_allocator()));
    resource_manager->add_resource_unit_loader(make_ref<ResourceUnitLoaderMaterial>(global_allocator()));
}

} // namespace WhiteBirdEngine
