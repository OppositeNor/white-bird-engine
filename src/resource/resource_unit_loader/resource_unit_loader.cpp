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
#include "core/parser/parser_json.hh"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "utils/utils.hh"
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

void resolve_image_data(IResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) {
    auto file_path = get_resource_path(p_scheme.path);
    uint32_t width, height, file_channels;
    auto image_data = load_image_data(file_path, p_scheme.channel_count, p_scheme.flip_h, p_scheme.flip_v, width, height, file_channels);
    if (image_data.empty()) {
        throw std::runtime_error(std::format("Failed to load image data from file: {}", static_cast<std::string>(file_path)));
    }
    p_resource_unit.width = width;
    p_resource_unit.height = height;
    p_resource_unit.channel_count = p_scheme.channel_count == 0 ? file_channels : p_scheme.channel_count;
    p_resource_unit.color_space = get_color_space_from_str(p_scheme.color_space);
    auto data = p_res_chunk.push_data(width * height * p_resource_unit.channel_count, image_data.data());
    p_resource_unit.image_data = data;
}

void resolve_hdr_image_data(IResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) {
    auto file_path = get_resource_path(p_scheme.path);
    uint32_t width, height, file_channels;
    auto image_data =
        load_image_data_hdr(file_path, p_scheme.channel_count, p_scheme.flip_h, p_scheme.flip_v, width, height, file_channels);
    if (image_data.has_value()) {
        throw std::runtime_error(std::format("Failed to load HDR image data from file: {}", static_cast<std::string>(file_path)));
    }
    p_resource_unit.width = width;
    p_resource_unit.height = height;
    p_resource_unit.channel_count = p_scheme.channel_count == 0 ? file_channels : p_scheme.channel_count;
    p_resource_unit.color_space = get_color_space_from_str(p_scheme.color_space);
    p_resource_unit.config.set(RUImage::IS_HDR);
    auto data = p_res_chunk.push_data(width * height * p_resource_unit.channel_count * sizeof(float), (*image_data).data());
    p_resource_unit.image_data = data;
}

template <typename LoaderType, typename SchemeType>
void populate_from_parser_data(const LoaderType& p_loader, IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) {
    SchemeType scheme;
    scheme.deserialize(p_scheme_data);
    p_loader.populate_scheme(p_res_chunk, scheme);
}

} // namespace

std::vector<std::string> ResourceUnitLoaderAudio::get_supported_resource_types() const {
    return {"audio"};
}

void ResourceUnitLoaderAudio::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeAudio>(p_scheme, "audio");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
}

void ResourceUnitLoaderAudio::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderAudio, RUSchemeAudio>(*this, p_res_chunk, p_scheme_data);
}

std::vector<std::string> ResourceUnitLoaderBinary::get_supported_resource_types() const {
    return {"binary"};
}

void ResourceUnitLoaderBinary::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeBinary>(p_scheme, "binary");
    auto file_path = get_resource_path(typed_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    RUBinary resource_unit;
    resource_unit.data = data;
    resource_unit.size = file_content.size();
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderBinary::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderBinary, RUSchemeBinary>(*this, p_res_chunk, p_scheme_data);
}

std::vector<std::string> ResourceUnitLoaderFont::get_supported_resource_types() const {
    return {"font"};
}

void ResourceUnitLoaderFont::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeFont>(p_scheme, "font");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
}

void ResourceUnitLoaderFont::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderFont, RUSchemeFont>(*this, p_res_chunk, p_scheme_data);
}

std::vector<std::string> ResourceUnitLoaderImage::get_supported_resource_types() const {
    return {"image"};
}

void ResourceUnitLoaderImage::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeImage>(p_scheme, "image");
    RUImage resource_unit;
    if (typed_scheme.is_hdr) {
        resolve_hdr_image_data(p_res_chunk, typed_scheme, resource_unit);
    } else {
        resolve_image_data(p_res_chunk, typed_scheme, resource_unit);
    }
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderImage::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderImage, RUSchemeImage>(*this, p_res_chunk, p_scheme_data);
}

std::vector<std::string> ResourceUnitLoaderText::get_supported_resource_types() const {
    return {"text"};
}

void ResourceUnitLoaderText::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeText>(p_scheme, "text");
    auto file_path = get_resource_path(typed_scheme.path);
    std::string file_content = load_text_file(static_cast<std::string>(file_path).c_str());
    auto data = populate_text(p_res_chunk, file_content);
    RUText resource_unit;
    resource_unit.text_data = data;
    resource_unit.size = file_content.size() + 1;
    resource_unit.encode_type = get_encode_type_from_str(typed_scheme.encode_type);
    p_res_chunk.push_res_unit(typed_scheme.id, resource_unit);
}

void ResourceUnitLoaderText::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderText, RUSchemeText>(*this, p_res_chunk, p_scheme_data);
}

std::vector<std::string> ResourceUnitLoaderVideo::get_supported_resource_types() const {
    return {"video"};
}

void ResourceUnitLoaderVideo::populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeVideo>(p_scheme, "video");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
}

void ResourceUnitLoaderVideo::populate_scheme(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    populate_from_parser_data<ResourceUnitLoaderVideo, RUSchemeVideo>(*this, p_res_chunk, p_scheme_data);
}

} // namespace WhiteBirdEngine
