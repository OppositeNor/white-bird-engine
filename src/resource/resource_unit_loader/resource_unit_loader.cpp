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
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "boost/algorithm/string/case_conv.hpp"
#include "core/parser/parser_json.hh"
#include "core/surface/3d_primitive.hh"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_defs.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <any>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace WhiteBirdEngine {
namespace {

std::string normalize_res_type(std::string p_type) {
    boost::to_upper(p_type);
    return p_type;
}

const ParserJSON::DataType& get_parser_data(const std::any& p_scheme_data, const std::string& p_res_type) {
    try {
        return std::any_cast<std::reference_wrapper<const ParserJSON::DataType>>(p_scheme_data).get();
    }
    catch (const std::bad_any_cast&) {
        throw std::runtime_error(std::format(
            "Failed to populate {} resource: parser data type mismatch.", p_res_type));
    }
}

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

RefRes<RUShader> populate_shader_resource(IResourceChunk& p_res_chunk, const RUSchemeShader& p_scheme) {
    ParserJSON parser;
    parser.parse(get_resource_path(p_scheme.metadata));
    ShaderMetadata shader_metadata;
    shader_metadata.deserialize(parser.get_data());
    // TODO: record metadata.
    auto file_path = get_resource_path(p_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    RUShader resource_unit;
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

RefRes<RUMaterial> resolve_material(IResourceChunk& p_res_chunk, const RUSchemeMaterial& p_material) {
    (void)p_res_chunk;
    (void)p_material;
    return RefRes<RUMaterial>(); // TODO
}

void resolve_image_data(IResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) {
    auto file_path = get_resource_path(p_scheme.path);
    uint32_t width, height, file_channels;
    auto image_data = load_image_data(
        file_path, p_scheme.channel_count, p_scheme.flip_h, p_scheme.flip_v,
        width, height, file_channels);
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
    auto image_data = load_image_data_hdr(
        file_path, p_scheme.channel_count, p_scheme.flip_h, p_scheme.flip_v,
        width, height, file_channels);
    if (image_data == nullptr) {
        throw std::runtime_error(std::format("Failed to load HDR image data from file: {}", static_cast<std::string>(file_path)));
    }
    p_resource_unit.width = width;
    p_resource_unit.height = height;
    p_resource_unit.channel_count = p_scheme.channel_count == 0 ? file_channels : p_scheme.channel_count;
    p_resource_unit.color_space = get_color_space_from_str(p_scheme.color_space);
    auto data = p_res_chunk.push_data(width * height * p_resource_unit.channel_count * sizeof(float), image_data.get());
    p_resource_unit.image_data = data;
}

void resolve_submesh_data(IResourceChunk& p_res_chunk, const RUSchemeSubmesh3D& p_scheme, RUSubmesh3D& p_resource_unit) {
    if (p_scheme.material_id.empty()) {
        p_res_chunk.create_ref_to(reinterpret_cast<void**>(&p_resource_unit.material), p_scheme.material_id);
    }
    else {
        p_resource_unit.material = resolve_material(p_res_chunk, p_scheme.material);
    }
    auto vertices_data = p_res_chunk.push_data(
        p_scheme.vertices_data.size() * sizeof(Vertex3DTextured), p_scheme.vertices_data.data());

    std::vector<Triangle3DIndxTextured> triangles(p_scheme.indices_data.size() / 3);
    for (size_t i = 0; i < triangles.size(); ++i) {
        auto& triangle = triangles.at(i);
        triangle.vert_array = vertices_data.reint_cast<Vertex3DTextured>();
        triangle.vertex_indices[0] = p_scheme.indices_data.at(i * 3);
        triangle.vertex_indices[1] = p_scheme.indices_data.at(i * 3 + 1);
        triangle.vertex_indices[2] = p_scheme.indices_data.at(i * 3 + 2);
    }

    auto triangles_data = p_res_chunk.push_data(triangles.size() * sizeof(Triangle3DIndxTextured), triangles.data());
    p_resource_unit.triangles = triangles_data.reint_cast<Triangle3DIndxTextured>();
    p_resource_unit.triangles_count = triangles.size();
}

template <typename LoaderType, typename SchemeType>
void populate_from_parser_data(const LoaderType& p_loader, IResourceChunk& p_res_chunk, const std::any& p_scheme_data, const std::string& p_type_name) {
    SchemeType scheme;
    scheme.deserialize(get_parser_data(p_scheme_data, p_type_name));
    p_loader.populate_scheme(p_res_chunk, scheme);
}

} // namespace

bool ResourceUnitLoaderAudio::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "AUDIO";
}

void ResourceUnitLoaderAudio::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeAudio>(p_scheme, "AUDIO");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
}

void ResourceUnitLoaderAudio::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderAudio, RUSchemeAudio>(*this, p_res_chunk, p_scheme_data, "AUDIO");
}

bool ResourceUnitLoaderBinary::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "BINARY";
}

void ResourceUnitLoaderBinary::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeBinary>(p_scheme, "BINARY");
    auto file_path = get_resource_path(typed_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    RUBinary resource_unit;
    resource_unit.data = data;
    resource_unit.size = file_content.size();
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderBinary::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderBinary, RUSchemeBinary>(*this, p_res_chunk, p_scheme_data, "BINARY");
}

bool ResourceUnitLoaderShader::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "SHADER";
}

void ResourceUnitLoaderShader::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeShader>(p_scheme, "SHADER");
    populate_shader_resource(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderShader::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderShader, RUSchemeShader>(*this, p_res_chunk, p_scheme_data, "SHADER");
}

bool ResourceUnitLoaderPipeline::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "PIPELINE";
}

void ResourceUnitLoaderPipeline::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemePipeline>(p_scheme, "PIPELINE");
    RUPipeline resource_unit;
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
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderPipeline::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderPipeline, RUSchemePipeline>(*this, p_res_chunk, p_scheme_data, "PIPELINE");
}

bool ResourceUnitLoaderMaterial::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "MATERIAL";
}

void ResourceUnitLoaderMaterial::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeMaterial>(p_scheme, "MATERIAL");
    resolve_material(p_res_chunk, typed_scheme);
}

void ResourceUnitLoaderMaterial::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderMaterial, RUSchemeMaterial>(*this, p_res_chunk, p_scheme_data, "MATERIAL");
}

bool ResourceUnitLoaderFont::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "FONT";
}

void ResourceUnitLoaderFont::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeFont>(p_scheme, "FONT");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
}

void ResourceUnitLoaderFont::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderFont, RUSchemeFont>(*this, p_res_chunk, p_scheme_data, "FONT");
}

bool ResourceUnitLoaderImage::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "IMAGE";
}

void ResourceUnitLoaderImage::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeImage>(p_scheme, "IMAGE");
    RUImage resource_unit;
    if (typed_scheme.is_hdr) {
        resolve_hdr_image_data(p_res_chunk, typed_scheme, resource_unit);
    }
    else {
        resolve_image_data(p_res_chunk, typed_scheme, resource_unit);
    }
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderImage::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderImage, RUSchemeImage>(*this, p_res_chunk, p_scheme_data, "IMAGE");
}

bool ResourceUnitLoaderMesh::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "MESH";
}

void ResourceUnitLoaderMesh::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeMesh3D>(p_scheme, "MESH");
    RUMesh3D resource_unit;
    std::vector<RefRes<RUSubmesh3D>> submeshes;
    submeshes.reserve(typed_scheme.submeshes.size() + typed_scheme.submeshes_id.size());
    for (const auto& submesh_scheme : typed_scheme.submeshes) {
        RUSubmesh3D submesh;
        resolve_submesh_data(p_res_chunk, submesh_scheme, submesh);
        auto saved_resource = p_res_chunk.push_res_unit("", submesh).reint_cast<RUSubmesh3D>();
        WBE_DEBUG_ASSERT(saved_resource != nullptr);
        submeshes.push_back(saved_resource);
    }
    // TODO: resolve submesh id
    size_t submesh_index_array_size_bytes = submeshes.size() * sizeof(RefRes<RUSubmesh3D>);
    auto submesh_array = p_res_chunk.push_data(submesh_index_array_size_bytes, submeshes.data()).reint_cast<RefRes<RUSubmesh3D>>();
    resource_unit.submeshes = submesh_array;
    resource_unit.submesh_count = submeshes.size();
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderMesh::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderMesh, RUSchemeMesh3D>(*this, p_res_chunk, p_scheme_data, "MESH");
}

bool ResourceUnitLoaderSubmesh::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "SUBMESH";
}

void ResourceUnitLoaderSubmesh::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeSubmesh3D>(p_scheme, "SUBMESH");
    if (typed_scheme.indices_data.size() % 3 != 0) {
        throw std::runtime_error(std::format(
            "Failed to populate submesh resource unit: indices data size {} is not a multiple of 3.",
            typed_scheme.indices_data.size()));
    }
    RUSubmesh3D resource_unit;
    resolve_submesh_data(p_res_chunk, typed_scheme, resource_unit);
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderSubmesh::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderSubmesh, RUSchemeSubmesh3D>(*this, p_res_chunk, p_scheme_data, "SUBMESH");
}

bool ResourceUnitLoaderText::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "TEXT";
}

void ResourceUnitLoaderText::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeText>(p_scheme, "TEXT");
    auto file_path = get_resource_path(typed_scheme.path);
    std::string file_content = load_text_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size() + 1, file_content.c_str());
    RUText resource_unit;
    resource_unit.text_data = data;
    resource_unit.size = file_content.size() + 1;
    resource_unit.encode_type = get_encode_type_from_str(typed_scheme.encode_type);
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderText::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderText, RUSchemeText>(*this, p_res_chunk, p_scheme_data, "TEXT");
}

bool ResourceUnitLoaderVideo::is_resource_supported(const RUScheme& p_scheme) const {
    return normalize_res_type(p_scheme.type) == "VIDEO";
}

void ResourceUnitLoaderVideo::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeVideo>(p_scheme, "VIDEO");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
}

void ResourceUnitLoaderVideo::populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderVideo, RUSchemeVideo>(*this, p_res_chunk, p_scheme_data, "VIDEO");
}

} // namespace WhiteBirdEngine