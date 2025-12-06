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
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "utils/utils.hh"
#include <cstdint>

namespace WhiteBirdEngine {

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeAudio& p_scheme) const {
    // TODO
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeBinary& p_scheme) const {
    auto file_path = get_resource_path(p_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    RUBinary resource_unit;
    resource_unit.data = data;
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeFont& p_scheme) const {
    // TODO
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme) const {
    auto file_path = get_resource_path(p_scheme.path);
    uint32_t width, height, file_channels;
    if (p_scheme.is_hdr) {
        auto image_data = load_image_data_hdr(file_path, p_scheme.channel_count,
                                              p_scheme.flip_h, p_scheme.flip_v,
                                              width, height, file_channels);
    }
    else{
        auto image_data = load_image_data(file_path, p_scheme.channel_count,
                                          p_scheme.flip_h, p_scheme.flip_v,
                                          width, height, file_channels);
    }
    RUImage resource_unit;
    resource_unit.width = width;
    resource_unit.height = height;
    resource_unit.channel_count = p_scheme.channel_count == 0 ? file_channels : p_scheme.channel_count;
    resource_unit.color_space = get_color_space_from_str(p_scheme.color_space);
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeMaterial& p_scheme) const {
    // TODO
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeSubmesh3D& p_scheme) const {
    // TODO
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeText& p_scheme) const {
    auto file_path = get_resource_path(p_scheme.path);
    std::string file_content = load_text_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size() + 1, file_content.c_str());
    RUText resource_unit;
    resource_unit.text_data = data;
    resource_unit.encode_type = get_encode_type_from_str(p_scheme.encode_type);
}

void RUPopulator::populate(ResourceChunk& p_res_chunk, const RUSchemeVideo& p_scheme) const {
    // TODO
}


}
