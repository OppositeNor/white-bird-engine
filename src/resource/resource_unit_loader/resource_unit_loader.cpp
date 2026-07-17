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
#include <any>
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
    auto result = p_res_chunk.push_data(p_str.size() + 1, p_str.data());
    result.get()[p_str.size()] = '\0';
    return result;
}

} // namespace

std::vector<std::string> ResourceUnitLoaderAudio::get_supported_resource_types() const {
    return {"audio"};
}

std::any ResourceUnitLoaderAudio::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeAudio>(p_scheme, "audio");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
    return std::any{};
}

void ResourceUnitLoaderAudio::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeAudio scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderBinary::get_supported_resource_types() const {
    return {"binary"};
}

std::any ResourceUnitLoaderBinary::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeBinary>(p_scheme, "binary");
    auto file_path = get_resource_path(typed_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    RUBinary resource_unit;
    resource_unit.data = data;
    resource_unit.size = file_content.size();
    return p_res_chunk.push_res_unit(typed_scheme.id, resource_unit).force_cast<RUBinary>();
}

void ResourceUnitLoaderBinary::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeBinary scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderFont::get_supported_resource_types() const {
    return {"font"};
}

std::any ResourceUnitLoaderFont::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeFont>(p_scheme, "font");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
    return std::any{};
}

void ResourceUnitLoaderFont::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeFont scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderImage::get_supported_resource_types() const {
    return {"image"};
}

std::any ResourceUnitLoaderImage::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeImage>(p_scheme, "image");
    auto file_path = get_resource_path(typed_scheme.path);
    uint32_t width, height, file_channels;
    RUImage resource_unit;
    if (typed_scheme.is_hdr) {
        auto image_data = load_image_data_hdr(
            file_path, typed_scheme.channel_count, typed_scheme.flip_h, typed_scheme.flip_v, width, height, file_channels);
        if (image_data.has_value()) {
            throw std::runtime_error(std::format("Failed to load HDR image data from file: {}", static_cast<std::string>(file_path)));
        }
        resource_unit.width = width;
        resource_unit.height = height;
        resource_unit.channel_count = typed_scheme.channel_count == 0 ? file_channels : typed_scheme.channel_count;
        resource_unit.color_space = get_color_space_from_str(typed_scheme.color_space);
        resource_unit.config.set(RUImage::IS_HDR);
        auto data = p_res_chunk.push_data(width * height * resource_unit.channel_count * sizeof(float), (*image_data).data());
        resource_unit.image_data = data;
    } else {
        auto image_data = load_image_data(
            file_path, typed_scheme.channel_count, typed_scheme.flip_h, typed_scheme.flip_v, width, height, file_channels);
        if (image_data.empty()) {
            throw std::runtime_error(std::format("Failed to load image data from file: {}", static_cast<std::string>(file_path)));
        }
        resource_unit.width = width;
        resource_unit.height = height;
        resource_unit.channel_count = typed_scheme.channel_count == 0 ? file_channels : typed_scheme.channel_count;
        resource_unit.color_space = get_color_space_from_str(typed_scheme.color_space);
        auto data = p_res_chunk.push_data(width * height * resource_unit.channel_count, image_data.data());
        resource_unit.image_data = data;
    }
    return p_res_chunk.push_res_unit(typed_scheme.id, resource_unit).force_cast<RUImage>();
}

void ResourceUnitLoaderImage::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeImage scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderText::get_supported_resource_types() const {
    return {"text"};
}

std::any ResourceUnitLoaderText::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeText>(p_scheme, "text");
    auto file_path = get_resource_path(typed_scheme.path);
    std::string file_content = load_text_file(static_cast<std::string>(file_path).c_str());
    auto data = populate_text(p_res_chunk, file_content);
    RUText resource_unit;
    resource_unit.text_data = data;
    resource_unit.size = file_content.size() + 1;
    resource_unit.encode_type = get_encode_type_from_str(typed_scheme.encode_type);
    return p_res_chunk.push_res_unit(typed_scheme.id, resource_unit).force_cast<RUText>();
}

void ResourceUnitLoaderText::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeText scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::vector<std::string> ResourceUnitLoaderVideo::get_supported_resource_types() const {
    return {"video"};
}

std::any ResourceUnitLoaderVideo::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemeVideo>(p_scheme, "video");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
    return std::any{};
}

void ResourceUnitLoaderVideo::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemeVideo scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

} // namespace WhiteBirdEngine
