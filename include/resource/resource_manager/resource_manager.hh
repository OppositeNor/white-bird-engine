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
#ifndef __WBE_RESOURCE_MANAGER_HH__
#define __WBE_RESOURCE_MANAGER_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/stack_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/parser/parser_json.hh"
#include "global/global.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/interface/singleton.hh"
#include <cstddef>
#include <stdexcept>
#include <string>
namespace WhiteBirdEngine {

/**
 * @class ResourceManager
 * @brief Manages the resources loaded and used by the engine.
 *
 * @todo Doc, Test
 */
class ResourceManager : public Singleton<ResourceManager> {
public:
    virtual ~ResourceManager() override {
        singleton = nullptr;
        resource_chunks.clear();
    }

    ResourceManager(size_t p_alloc_size, const std::string& p_global_res_path)
        : stack_allocator(p_alloc_size) {
        singleton = this;
        global_res_chunk_id = register_chunk(make_ref<ResourceChunkLoadFile<ParserJSON>>(
            global_allocator(), get_resource_path("res_chunks/global_resources.json"), stack_allocator));
    }

    /**
     * @brief Register a resource chunk.
     *
     * @param p_chunk_id The id of the chunk, used to locate the chunk when getting
     * resources.
     * @param p_resource_chunk The resource chunk to register.
     */
    ResourceChunkID register_chunk(Ref<ResourceChunk> p_resource_chunk) {
        ResourceChunkID chunk_id = resource_chunks.size();
        resource_chunks.push_back(p_resource_chunk);
        return chunk_id;
    }

    /**
     * @brief Pop a resource chunk from the end of the chunk stack.
     */
    void pop_chunk() {
        resource_chunks.pop_back();
    }

    /**
     * @brief Get a resource from a chunk.
     *
     * @param p_chunk The resource chunk to get resource from.
     * @param p_key The key of the resource.
     */
    template <ResourceUnitConcept T>
    T* get_resource(ResourceChunkID p_chunk_id, const std::string& p_key) {
        return reinterpret_cast<T*>(get_resource_chunk(p_chunk_id)->get_resource(p_key).get());
    }

    Ref<ResourceChunk> get_resource_chunk(ResourceChunkID p_chunk_id) {
        if (p_chunk_id >= resource_chunks.size()) {
            throw std::runtime_error("Failed to get resource chunk: chunk with ID " + std::to_string(p_chunk_id) + " not registered.");
        }
        return resource_chunks[p_chunk_id];
    }

    Ref<const ResourceChunk> get_resource_chunk(ResourceChunkID p_chunk_id) const {
        if (p_chunk_id >= resource_chunks.size()) {
            throw std::runtime_error("Failed to get resource chunk: chunk with ID " + std::to_string(p_chunk_id) + " not registered.");
        }
        return resource_chunks[p_chunk_id];
    }

    /**
     * @brief Get a resource by its memory ID.
     *
     * @param p_memid The memory ID of the resource.
     */
    template <ResourceUnitConcept T>
    T* get_res_by_memid(MemID p_memid) {
        return stack_allocator.get_obj<T>(p_memid);
    }

    /**
     * @brief Get the singleton instance.
     *
     * @return The singleton instance. Null if not initialized.
     */
    static ResourceManager* get_singleton() {
        return singleton;
    }

    ResourceChunkID global_res_chunk() const {
        return global_res_chunk_id;
    }

private:
    ResourceChunkID global_res_chunk_id;
    std::vector<Ref<ResourceChunk>> resource_chunks;
    StackAllocator stack_allocator;

    inline static ResourceManager* singleton = nullptr;
};

}

#endif
