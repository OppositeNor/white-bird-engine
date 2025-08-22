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
#ifndef __WBE_RESOURCE_CHUNK_HH__
#define __WBE_RESOURCE_CHUNK_HH__

#include "core/allocator/allocator.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"

namespace WhiteBirdEngine {

class ResourceManager;

using ResourceChunkID = HashCode;

/**
 * @class ResourceChunk
 * @brief Chunk of resources. Used for containing multiple resources to be loaded and deloaded at once.
 * @todo Test
 */
template <typename ChildT>
class ResourceChunk {
public:
    ResourceChunk() {}
    virtual ~ResourceChunk() {}

    /**
     * @brief Get the resource of a specific key.
     *
     * @param p_key The key to get resource from.
     * @return The MemID of the resource.
     */
    template <typename T>
    MemID get_resource(const std::string& p_key) const {
        return static_cast<ChildT*>(this)->get_resource(p_key);
    }

    /**
     * @brief Get the total size of the chunk.
     *
     * @return The total size of the chunk.
     */
    size_t get_size() const {
        return static_cast<ChildT*>(this)->get_size();
    }

    /**
     * @brief Push a resource unit to the chunk.
     *
     * @tparam T The type of the resource unit.
     * @param p_res_key The key of the resource unit.
     * @param p_allocator The allocator to allocate memory.
     * @param p_res The resource unit instance.
     * @return The memory id of the pushed resource unit.
     */
    template <ResourceUnitConcept T>
    MemID push_res_unit(const std::string& p_res_key, const T& p_res) {
        return static_cast<ChildT*>(this)->push_res_unit(p_res_key, p_res);
    }

    /**
     * @brief Push a chunk of data into the chunk. The data will be copied to the allocator.
     *
     * @param p_res_size The size of the resource to push to.
     * @param p_res The pointer to the resource to be pushed into the chunk.
     * @param p_allocator The allocator to allocate memory.
     * @return The result memory id.
     */
    MemID push_data(size_t p_res_size, void* p_res) {
        return static_cast<ChildT*>(this)->push_data(p_res_size, p_res);
    }

};

WBE_DECL_CRTP_CONCEPT(ResourceChunk);

}

#endif
