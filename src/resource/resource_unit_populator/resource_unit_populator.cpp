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
#include "resource/resource_unit_populator/resource_unit_populator.hh"
#include "core/allocator/allocator.hh"
#include "core/logging/log.hh"
#include "core/surface/3d_primitive.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <cstring>
#include <format>
#include <memory>

namespace WhiteBirdEngine {

RUPopulator::RUPopulator() {
    resource_manager = ResourceManager::get_singleton();
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeAudio& p_scheme) const {
    // TODO
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeBinary& p_scheme) const {
    auto file_path = get_resource_path(p_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    RUBinary resource_unit;
    resource_unit.data = data;
    resource_unit.size = file_content.size();
    p_res_chunk.push_res_unit(p_scheme.id, resource_unit);
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeFont& p_scheme) const {
    // TODO
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme) const {
    auto file_path = get_resource_path(p_scheme.path);
    std::unique_ptr<unsigned char> image_data;
    RUImage resource_unit;
    if (p_scheme.is_hdr) {
        resolve_hdr_image_data(p_res_chunk, p_scheme, resource_unit);
    }
    else{
        resolve_image_data(p_res_chunk, p_scheme, resource_unit);
    }
    p_res_chunk.push_res_unit(p_scheme.id, resource_unit);
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeMaterial& p_scheme) const {
    resolve_material(p_res_chunk, p_scheme);
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeMesh3D& p_scheme) const {
    // TODO
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeSubmesh3D& p_scheme) const {
    if (p_scheme.indices_data.size() % 3 != 0) {
        throw std::runtime_error(std::format(
            "Failed to populate submesh resource unit: indices data size {} is not a multiple of 3.",
            p_scheme.indices_data.size()));
    }
    RUSubmesh3D resource_unit;
    if (p_scheme.material_id == "") {
        p_res_chunk.create_ref_to(reinterpret_cast<void**>(&resource_unit.material), p_scheme.material_id);
    }
    else {
        resource_unit.material = resolve_material(p_res_chunk, p_scheme.material);
    }
    auto vertices_data = p_res_chunk.push_data(
        p_scheme.vertices_data.size() * sizeof(Vertex3DTextured), p_scheme.vertices_data.data());

    std::vector<Triangle3DIndxTextured> triangles(p_scheme.indices_data.size() / 3);
    for (size_t i = 0; i < triangles.size(); ++i) {
        triangles[i].vert_array = vertices_data.reint_cast<Vertex3DTextured>();
        triangles[i].vertex_indices[0] = p_scheme.indices_data[i * 3];
        triangles[i].vertex_indices[1] = p_scheme.indices_data[i * 3 + 1];
        triangles[i].vertex_indices[2] = p_scheme.indices_data[i * 3 + 2];
    }

    auto triangles_data = p_res_chunk.push_data(triangles.size() * sizeof(Triangle3DIndxTextured), triangles.data());
    resource_unit.triangles = triangles_data.reint_cast<Triangle3DIndxTextured>();
    resource_unit.triangles_count = triangles.size();
    wbe_console_log()->message(std::format("Loaded mesh: {}, {} triangles.", p_scheme.id, triangles.size()));
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeText& p_scheme) const {
    auto file_path = get_resource_path(p_scheme.path);
    std::string file_content = load_text_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size() + 1, file_content.c_str());
    RUText resource_unit;
    resource_unit.text_data = data;
    resource_unit.size = file_content.size() + 1;
    resource_unit.encode_type = get_encode_type_from_str(p_scheme.encode_type);
    p_res_chunk.push_res_unit(p_scheme.id, resource_unit);
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeVideo& p_scheme) const {
    // TODO
}

void RUPopulator::resolve_image_data(ResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) const {
    auto file_path = get_resource_path(p_scheme.path);
    uint32_t width, height, file_channels;
    auto image_data = load_image_data(
        file_path, p_scheme.channel_count, p_scheme.flip_h, p_scheme.flip_v,
        width, height, file_channels);
    p_resource_unit.width = width;
    p_resource_unit.height = height;
    p_resource_unit.channel_count = p_scheme.channel_count == 0 ? file_channels : p_scheme.channel_count;
    p_resource_unit.color_space = get_color_space_from_str(p_scheme.color_space);
    auto data = p_res_chunk.push_data(width * height * p_resource_unit.channel_count, image_data.get());
    p_resource_unit.image_data = data;
}

void RUPopulator::resolve_hdr_image_data(ResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) const {
    auto file_path = get_resource_path(p_scheme.path);
    uint32_t width, height, file_channels;
    auto image_data = load_image_data_hdr(
        file_path, p_scheme.channel_count,  p_scheme.flip_h, p_scheme.flip_v,
        width, height, file_channels);
    p_resource_unit.width = width;
    p_resource_unit.height = height;
    p_resource_unit.channel_count = p_scheme.channel_count == 0 ? file_channels : p_scheme.channel_count;
    p_resource_unit.color_space = get_color_space_from_str(p_scheme.color_space);
    auto data = p_res_chunk.push_data(width * height * p_resource_unit.channel_count * sizeof(float), image_data.get());
    p_resource_unit.image_data = data;
}

RefRes<RUMaterial> RUPopulator::resolve_material(ResourceChunk& p_res_chunk, const RUSchemeMaterial& p_material) const {
    (void)p_res_chunk, (void)p_material;
    return MEM_NULL; // TODO
}

} // namespace WhiteBirdEngine
