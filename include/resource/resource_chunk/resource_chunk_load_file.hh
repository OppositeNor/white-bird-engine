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
#ifndef WBE_FILE_RESOURCE_CHUNK_LOADER_HH
#define WBE_FILE_RESOURCE_CHUNK_LOADER_HH
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stack_allocator.hh"
#include "core/parser/parser.hh"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include <cstring>
#include <format>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class ResourceChunk
 * @brief Chunk of resources. Used for containing multiple resources to be loaded and deloaded at once.
 * @todo Test
 */
template <ParserConcept ParserType>
class ResourceChunkLoadFile final : public IResourceChunk {
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
        return resource_map.contains(p_res_key);
    }

    virtual void create_ref_to(void** p_ptr, const std::string& p_id) override {
        ref_map[p_ptr] = p_id;
    }

    virtual std::vector<std::string> get_all_keys() const override {
        std::vector<std::string> result;
        result.reserve(resource_map.size());
        for (const auto& [key, _] : resource_map) {
            result.push_back(key);
        }
        return result;
    }

protected:

    using ParserDataType = ParserType::DataType;

    virtual RefRes<char> push_res_unit_ptr(const std::string& p_res_key, const void* p_res, size_t p_size) override {
        if (!p_res_key.empty() && resource_map.contains(p_res_key)) {
            throw std::runtime_error(std::format("Redefinition of key: {}.", p_res_key));
        }
        total_size += p_size;
        MemID result_mem_id = allocator->allocate(p_size);
        memcpy(allocator->get(result_mem_id), p_res, p_size);
        RefRes<char> result(result_mem_id, allocator);
        if (!p_res_key.empty()) {
            resource_map[p_res_key.c_str()] = result;
        }
        return result;
    }
private:
    size_t total_size = 0;
    std::unordered_map<std::string, RefRes<char>> resource_map;
    std::unordered_map<void**, std::string> ref_map;
    StackAllocator* allocator;

    void resolve_load_resource(std::set<std::string>& p_included_pathes, const std::string& p_res_chunk_path);

    void resolve_refs() {
        auto* resource_manager = ResourceManager::get_singleton();
        for (auto& [ptr, key] : ref_map) {
            *ptr = resource_manager->get_resource<char>(key).get();
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
        RUScheme resource_scheme;
        resource_scheme.deserialize(res);
        const auto& resource_unit_loaders = ResourceManager::get_singleton()->get_resource_unit_loaders();
        bool is_resource_supported = false;
        for (const auto& resource_unit_loader : resource_unit_loaders) {
            if (!resource_unit_loader->is_resource_supported(resource_scheme)) {
                continue;
            }
            resource_unit_loader->populate_scheme(*this, std::cref(res));
            is_resource_supported = true;
            break;
        }
        if (!is_resource_supported) {
            throw std::runtime_error(std::format("Unrecognized resource type: {}.", resource_scheme.type));
        }
    }
    resolve_refs();
}

template <ParserConcept ParserType>
ResourceChunkLoadFile<ParserType>::ResourceChunkLoadFile(const std::string& p_path, StackAllocator& p_allocator) {
    allocator = &p_allocator;
    std::set<std::string> included_pathes;
    resolve_load_resource(included_pathes, p_path);
    resolve_refs();
}

} // namespace WhiteBirdEngine

#endif
