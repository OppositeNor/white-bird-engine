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
#ifndef __WBE_RESOURCE_CHUNK_LOADER_HH__
#define __WBE_RESOURCE_CHUNK_LOADER_HH__
#include "core/allocator/stack_allocator.hh"
#include "core/parser/parser.hh"
#include "platform/file_system/path.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_unit_loader/ru_loader_binary.hh"
#include "resource/resource_unit_loader/ru_loader_image.hh"
#include "resource/resource_unit_loader/ru_loader_text.hh"
#include <cstring>
#include <string>
#include <unordered_map>

namespace WhiteBirdEngine {

/**
 * @class ResourceChunk
 * @brief Chunk of resources. Used for containing multiple resources to be loaded and deloaded at once.
 * @todo Test
 */
template <ParserConcept ParserType>
class ResourceChunkLoadFile final : public ResourceChunk {
public:
    ~ResourceChunkLoadFile() {
        allocator->pop_stack(total_size);
    }
    ResourceChunkLoadFile(const ResourceChunkLoadFile&) = delete;
    ResourceChunkLoadFile(ResourceChunkLoadFile&&) = delete;
    ResourceChunkLoadFile& operator=(const ResourceChunkLoadFile&) = delete;
    ResourceChunkLoadFile& operator=(ResourceChunkLoadFile&&) = delete;

    /**
     * @brief Constructor
     *
     * @param p_parser The parser to get resource from.
     */
    ResourceChunkLoadFile(const Path& p_path, StackAllocator& p_allocator);

    virtual MemID get_resource(const std::string& p_key) const override {
        return resource_map.at(p_key.c_str());
    }

    virtual MemID push_data(size_t p_res_size, void* p_res) override {
        total_size += p_res_size;
        MemID result = allocator->allocate(p_res_size);
        memcpy(allocator->get(result), p_res, p_res_size);
        return result;
    }

    bool is_already_registered(const std::string& p_res_key) {
        return resource_map.contains(p_res_key.c_str());
    }

    virtual size_t get_size() const override {
        return total_size;
    }

    virtual void* get_data(MemID p_mem_id) override {
        return allocator->get(p_mem_id);
    }

    virtual const void* get_data(MemID p_mem_id) const override {
        return allocator->get(p_mem_id);
    }

protected:

    virtual MemID push_res_unit_ptr(const std::string& p_res_key, const void* p_res, size_t p_size) override {
        total_size += p_size;
        MemID result = allocator->allocate(p_size);
        memcpy(allocator->get(result), p_res, p_size);
        resource_map[p_res_key.c_str()] = result;
        return result;
    }

private:
    size_t total_size;
    std::unordered_map<std::string, MemID> resource_map;
    StackAllocator* allocator;

    enum class RUType {
        AUDIO,
        BINARY,
        FONT,
        IMAGE,
        TEXT,
        VIDEO
    };

    RUType get_res_type(const std::string& p_type);

    void load_ru_image(const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator);
    void load_ru_audio(const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator);
    void load_ru_video(const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator);
    void load_ru_text(const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator);
    void load_ru_font(const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator);
    void load_ru_binary(const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key,  StackAllocator& p_allocator);
};

template <ParserConcept ParserType>
ResourceChunkLoadFile<ParserType>::ResourceChunkLoadFile(const Path& p_path, StackAllocator& p_allocator) {
    allocator = &p_allocator;
    ParserType parser;
    parser.parse(p_path);
    std::vector<std::string> keys = parser.get_all_keys();
    for (auto& key : keys) {
        auto res_type = get_res_type(parser.template get_value<typename ParserType::DataType>(key).template get_value<std::string>("type"));
        switch (res_type) {
            case RUType::AUDIO:
                load_ru_audio(parser.template get_value<typename ParserType::DataType>(key), key, p_allocator);
                break;
            case RUType::BINARY:
                load_ru_binary(parser.template get_value<typename ParserType::DataType>(key), key, p_allocator);
                break;
            case RUType::FONT:
                load_ru_font(parser.template get_value<typename ParserType::DataType>(key), key, p_allocator);
                break;
            case RUType::IMAGE:
                load_ru_image(parser.template get_value<typename ParserType::DataType>(key), key, p_allocator);
                break;
            case RUType::TEXT:
                load_ru_text(parser.template get_value<typename ParserType::DataType>(key), key, p_allocator);
                break;
            case RUType::VIDEO:
                load_ru_video(parser.template get_value<typename ParserType::DataType>(key), key, p_allocator);
                break;
        }
    }
}

template <ParserConcept ParserType>
void ResourceChunkLoadFile<ParserType>::load_ru_image(
    const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator) {
    RULoaderImage loader;
    loader.load<typename ParserType::DataType>(*this, p_key, p_parser_data);
}

template <ParserConcept ParserType>
void ResourceChunkLoadFile<ParserType>::load_ru_audio(
    const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator) {
    // TODO
}

template <ParserConcept ParserType>
void ResourceChunkLoadFile<ParserType>::load_ru_video(
    const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator) {
    // TODO
}

template <ParserConcept ParserType>
void ResourceChunkLoadFile<ParserType>::load_ru_text(
    const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator) {
    RULoaderText loader;
    loader.load<typename ParserType::DataType>(*this, p_key, p_parser_data);
}

template <ParserConcept ParserType>
void ResourceChunkLoadFile<ParserType>::load_ru_font(
    const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator) {
    // TODO
}

template <ParserConcept ParserType>
void ResourceChunkLoadFile<ParserType>::load_ru_binary(
    const ParserData<typename ParserType::DataType>& p_parser_data, const std::string& p_key, StackAllocator& p_allocator) {
    RULoaderBinary loader;
    loader.load<typename ParserType::DataType>(*this, p_key, p_parser_data);
}

template <ParserConcept ParserType>
ResourceChunkLoadFile<ParserType>::RUType ResourceChunkLoadFile<ParserType>::get_res_type(const std::string& p_type) {
    if (p_type == "audio") {
        return RUType::AUDIO;
    }
    if (p_type == "binary") {
        return RUType::BINARY;
    }
    if (p_type == "font") {
        return RUType::FONT;
    }
    if (p_type == "image") {
        return RUType::IMAGE;
    }
    if (p_type == "text") {
        return RUType::TEXT;
    }
    if (p_type == "video.") {
        return RUType::VIDEO;
    }
    throw std::runtime_error("Failed to get resource type. Type: " + p_type + " not recognized.");
}

}

#endif
