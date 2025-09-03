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
#include "resource/resource_chunk/resource_chunk.hh"
#include "utils/utils.hh"
#include <cstring>
#include <unordered_map>

namespace WhiteBirdEngine {

/**
 * @class ResourceChunk
 * @brief Chunk of resources. Used for containing multiple resources to be loaded and deloaded at once.
 * @todo Test
 */
class ResourceChunkLoadFile : public ResourceChunk<ResourceChunkLoadFile> {
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
    template <ParserConcept ParserType>
    ResourceChunkLoadFile(const Parser<ParserType>& p_parser, StackAllocator& p_allocator);

    MemID get_resource(const std::string& p_key) const {
        return resource_map.at(dynam_hash(p_key.c_str()));
    }

    size_t get_size() const {
        return total_size;
    }

    template <ResourceUnitConcept T>
    MemID push_res_unit(const std::string& p_res_key, const T& p_res) {
        using PureT = std::remove_cvref<T>;
        total_size += sizeof(PureT);
        MemID result = allocator->allocate(sizeof(PureT));
        *(allocator->get_obj<PureT>(result)) = p_res;
        resource_map[dynam_hash(p_res_key.c_str())] = result;
        return result;
    }

    MemID push_data(size_t p_res_size, void* p_res) {
        total_size += p_res_size;
        MemID result = allocator->allocate(p_res_size);
        memcpy(allocator->get(result), p_res, p_res_size);
        return result;
    }

    bool is_already_registered(const std::string& p_res_key) {
        return resource_map.contains(dynam_hash(p_res_key.c_str()));
    }

private:
    size_t total_size;
    std::unordered_map<HashCode, MemID> resource_map;
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

    template <ParserDataConcept ParserDataType>
    void load_ru_image(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_audio(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_video(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_text(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_font(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_binary(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
};

}

#endif
