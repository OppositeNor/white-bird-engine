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
#include "boost/algorithm/string/case_conv.hpp"
#include "core/allocator/stack_allocator.hh"
#include "core/parser/parser.hh"
#include "core/reflection/reflection_defs.hh"
#include "generated/label_manager.gen.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_populator/resource_unit_populator.hh"
#include "resource/resource_utils.hh"
#include <cstring>
#include <format>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace WhiteBirdEngine {

// Resource unit labels.
WBE_LABEL(AUDIO, WBE_RESOURCE_UNIT);
WBE_LABEL(BINARY, WBE_RESOURCE_UNIT);
WBE_LABEL(MATERIAL, WBE_RESOURCE_UNIT);
WBE_LABEL(FONT, WBE_RESOURCE_UNIT);
WBE_LABEL(IMAGE, WBE_RESOURCE_UNIT);
WBE_LABEL(MESH, WBE_RESOURCE_UNIT);
WBE_LABEL(SUBMESH, WBE_RESOURCE_UNIT);
WBE_LABEL(TEXT, WBE_RESOURCE_UNIT);
WBE_LABEL(VIDEO, WBE_RESOURCE_UNIT);
using RUType = HashCode;

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
     * @param p_path The resource path to the resource file.
     * @param p_parser The parser to get resource from.
     */
    ResourceChunkLoadFile(const std::string& p_path, StackAllocator& p_allocator);

    virtual RefRes<char> get_resource(const std::string& p_id) const override {
        return resource_map.at(p_id);
    }

    virtual RefRes<char> push_data(size_t p_res_size, const void* p_res) override {
        total_size += p_res_size;
        MemID result_mem_id = allocator->allocate(p_res_size);
        memcpy(allocator->get(result_mem_id), p_res, p_res_size);
        RefRes<char> result(result_mem_id, allocator);
        return result;
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

    virtual bool contains(const std::string& p_res_key) const override {
        return resource_map.find(p_res_key) != resource_map.end();
    }

    virtual void create_ref_to(void** p_ptr, const std::string& p_id) override {
        ref_map[p_ptr] = p_id;
    }

    virtual std::vector<std::string> get_all_keys() const override {
        std::vector<std::string> result;
        for (const auto& [key, _] : resource_map) {
            result.push_back(key);
        }
        return result;
    }

protected:

    using ParserDataType = ParserType::DataType;

    virtual RefRes<char> push_res_unit_ptr(const std::string& p_res_key, const void* p_res, size_t p_size) override {
        if (p_res_key != "" && resource_map.find(p_res_key) != resource_map.end()) {
            throw std::runtime_error(std::format("Redefinition of key: {}.", p_res_key));
        }
        total_size += p_size;
        MemID result_mem_id = allocator->allocate(p_size);
        memcpy(allocator->get(result_mem_id), p_res, p_size);
        RefRes<char> result(result_mem_id, allocator);
        if (p_res_key != "") {
            resource_map[p_res_key.c_str()] = result;
        }
        return result;
    }
private:
    size_t total_size;
    std::unordered_map<std::string, RefRes<char>> resource_map;
    std::unordered_map<void**, std::string> ref_map;
    StackAllocator* allocator;

    void resolve_load_resource(std::set<std::string>& p_included_pathes, const std::string& p_res_chunk_path);
    RUType get_res_type(std::string p_type);

    RUPopulator populator;

    template <typename RUSchemeType>
    void populate_scheme(const ParserDataType& p_parser_data);

    void resolve_refs() {
        auto resource_manager = ResourceManager::get_singleton();
        for (auto& [ptr, key] : ref_map) {
            *ptr = resource_manager->get_resource<void>(key);
        }
    }
};

template <ParserConcept ParserType>
void ResourceChunkLoadFile<ParserType>::resolve_load_resource(std::set<std::string>& p_included_pathes, const std::string& p_res_chunk_path) {
    if (p_included_pathes.contains(p_res_chunk_path)) {
        throw std::runtime_error(std::format("Include recursion found: {}", p_res_chunk_path));
    }
    p_included_pathes.insert(p_res_chunk_path);

    auto rc_file_path = get_resource_path(p_res_chunk_path);
    ParserType parser;
    parser.parse(rc_file_path);

    if (parser.contains("include")) {
        // Resolve includes
        auto included_resource_chunks = parser.template get_value<std::vector<std::string>>("include");
        for (auto& include_path : included_resource_chunks) {
            resolve_load_resource(p_included_pathes, include_path);
        }
    }

    std::vector<ParserDataType> resources = parser.template get_value<std::vector<ParserDataType>>("resources");
    for (auto& res : resources) {
        auto res_type = get_res_type(res.template get_value<std::string>("type"));
        switch (res_type) {
            case AUDIO:
                populate_scheme<RUSchemeAudio>(res);
                break;
            case BINARY:
                populate_scheme<RUSchemeBinary>(res);
                break;
            case FONT:
                populate_scheme<RUSchemeFont>(res);
                break;
            case IMAGE:
                populate_scheme<RUSchemeImage>(res);
                break;
            case MATERIAL:
                populate_scheme<RUSchemeMaterial>(res);
                break;
            case MESH:
                populate_scheme<RUSchemeMesh>(res);
                break;
            case SUBMESH:
                populate_scheme<RUSchemeSubmesh3D>(res);
                break;
            case TEXT:
                populate_scheme<RUSchemeText>(res);
                break;
            case VIDEO:
                populate_scheme<RUSchemeVideo>(res);
                break;
        }
    }
    resolve_refs();
}

template <ParserConcept ParserType>
template <typename RUSchemeType>
void ResourceChunkLoadFile<ParserType>::populate_scheme(const ParserDataType& p_parser_data) {
    RUSchemeType scheme;
    scheme.deserialize(p_parser_data);
    populator.populate(*this, scheme);
}

template <ParserConcept ParserType>
ResourceChunkLoadFile<ParserType>::ResourceChunkLoadFile(const std::string& p_path, StackAllocator& p_allocator) {
    allocator = &p_allocator;
    std::set<std::string> included_pathes;
    resolve_load_resource(included_pathes, p_path);
    resolve_refs();
}

template <ParserConcept ParserType>
RUType ResourceChunkLoadFile<ParserType>::get_res_type(std::string p_type) {
    boost::to_upper(p_type);
    return LabelManager::get_singleton()->get_label_from_name(p_type);
}

}

#endif
