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
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_unit_loader/ru_loader_binary.hh"
#include "resource/resource_unit_loader/ru_loader_image.hh"
#include "resource/resource_unit_loader/ru_loader_text.hh"
#include <stdexcept>

namespace WhiteBirdEngine {
ResourceChunkLoadFile::RUType ResourceChunkLoadFile::get_res_type(const std::string& p_type) {
    if (p_type.starts_with("audio.")) {
        return RUType::AUDIO;
    }
    if (p_type.starts_with("binary.")) {
        return RUType::BINARY;
    }
    if (p_type.starts_with("font.")) {
        return RUType::FONT;
    }
    if (p_type.starts_with("image.")) {
        return RUType::IMAGE;
    }
    if (p_type.starts_with("text.")) {
        return RUType::TEXT;
    }
    if (p_type.starts_with("video.")) {
        return RUType::VIDEO;
    }
    throw std::runtime_error("Failed to get resource type. Type: " + p_type + " not recognized.");
}

template <ParserConcept ParserType>
ResourceChunkLoadFile::ResourceChunkLoadFile(const Parser<ParserType>& p_parser, StackAllocator& p_allocator) {
    allocator = &p_allocator;
    std::vector<std::string> keys = p_parser.get_all_keys();
    for (auto& key : keys) {
        auto res_type = get_res_type(key);
        switch (res_type) {
            case RUType::AUDIO:
                load_ru_audio(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::BINARY:
                load_ru_binary(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::FONT:
                load_ru_font(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::IMAGE:
                load_ru_image(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::TEXT:
                load_ru_text(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::VIDEO:
                load_ru_video(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
        }
    }
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_image(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    RULoaderImage loader;
    loader.load<ParserDataType>(*this, p_parser_data);
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_audio(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_video(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_text(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    RULoaderText loader;
    loader.load<ParserDataType>(*this, p_parser_data.get_data());
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_font(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_binary(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    RULoaderBinary loader;
    loader.load<ParserDataType>(*this, p_parser_data.get_data());
}
}
