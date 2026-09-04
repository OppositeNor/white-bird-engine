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
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <any>
#include <array>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {
namespace {

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
    std::string nul_terminated_text(p_str);
    nul_terminated_text.push_back('\0');
    return p_res_chunk.push_data(nul_terminated_text.size(), nul_terminated_text.data());
}

} // namespace

std::span<const std::string> ResourceUnitLoaderAudio::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"audio"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderAudio::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaAudio>(p_scheme, "audio");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
    return std::any{};
}

void ResourceUnitLoaderAudio::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaAudio scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderBinary::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"binary"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderBinary::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaBinary>(p_scheme, "binary");
    auto file_path = get_resource_path(typed_scheme.path);
    auto file_content = load_binary_file(static_cast<std::string>(file_path).c_str());
    auto data = p_res_chunk.push_data(file_content.size(), file_content.data());
    RUBinary resource_unit;
    resource_unit.data = data;
    resource_unit.size = file_content.size();
    return p_res_chunk.push_res_unit(typed_scheme.id, resource_unit).force_cast<RUBinary>();
}

void ResourceUnitLoaderBinary::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaBinary scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderFont::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"font"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderFont::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaFont>(p_scheme, "font");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
    return std::any{};
}

void ResourceUnitLoaderFont::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaFont scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderImage::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"image"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderImage::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaImage>(p_scheme, "image");
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
        auto data = p_res_chunk.push_data(image_data.size(), image_data.data());
        WBE_DEBUG_ASSERT(width * height * resource_unit.channel_count == image_data.size());
        resource_unit.image_data = data;
    }
    return p_res_chunk.push_res_unit(typed_scheme.id, resource_unit).force_cast<RUImage>();
}

void ResourceUnitLoaderImage::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaImage scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderTexture::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"texture"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderTexture::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaTexture>(p_scheme, "texture");
    auto file_path = get_resource_path(typed_scheme.path);
    return p_res_chunk.emplace_res_unit<RUTexture>(typed_scheme.id, static_cast<std::string>(file_path));
}

void ResourceUnitLoaderTexture::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaTexture scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderText::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"text"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderText::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaText>(p_scheme, "text");
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
    RUSchemaText scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

std::span<const std::string> ResourceUnitLoaderVideo::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"video"};
    return supported_resource_types;
}

std::any ResourceUnitLoaderVideo::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
    const auto& typed_scheme = get_typed_scheme<RUSchemaVideo>(p_scheme, "video");
    (void)p_res_chunk;
    (void)typed_scheme;
    // TODO
    return std::any{};
}

void ResourceUnitLoaderVideo::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    RUSchemaVideo scheme;
    scheme.deserialize(p_scheme_data);
    populate_scheme_ptr(p_res_chunk, scheme);
}

} // namespace WhiteBirdEngine
