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
#ifndef WBE_FILE_I_RESOURCE_CHUNK_HH
#define WBE_FILE_I_RESOURCE_CHUNK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/unique.hh"
#include "core/object/object.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace WhiteBirdEngine {

class ResourceManager;

using ResourceChunkID = HashCode;
constexpr ResourceChunkID RES_CHUNK_NULL = 0x0;

/**
 * @class IResourceChunk
 * @brief Interface for chunk of resources. Used for containing multiple
 * resources to be loaded and deloaded at once.
 * @todo Test
 */
class IResourceChunk {
public:
    WBE_R6_DEFAULT_VIRTUAL(IResourceChunk)

    /**
     * @brief Get the resource from a key.
     *
     * @param p_id The identifier of the resource unit.
     * @return The resource reference of the resource.
     */
    virtual RefRes<char> get_resource(const std::string& p_id) const = 0;

    /**
     * @brief Look up a resource by key without throwing.
     *
     * Performs a single lookup and returns the resource if present, avoiding the
     * need to call contains() followed by get_resource().
     *
     * @param p_id The identifier of the resource unit.
     * @return The resource reference, or a null reference when the chunk does
     * not contain the resource.
     */
    virtual RefRes<char> try_get_resource(const std::string& p_id) const = 0;

    /**
     * @brief Get the total size of the chunk.
     *
     * @return The total size of the chunk.
     */
    virtual size_t get_size() const = 0;

    /**
     * @brief Push a chunk of data into the chunk. The data will be copied to
     * the allocator.
     *
     * @param p_res_size The size of the resource to push to.
     * @param p_res The pointer to the resource to be pushed into the chunk.
     * @param p_allocator The allocator to allocate memory.
     * @return The result memory id.
     */
    virtual RefRes<char> push_data(size_t p_res_size, const void* p_res) = 0;

    /**
     * @brief Register an object to the chunk, which it will be destructed when
     * the chunk is unloaded.
     *
     * @param p_object The object to register.
     * @return The resource reference to the object.
     */
    virtual RefRes<Object> register_object(Unique<Object>&& p_object) = 0;

    /**
     * @brief Get the data at ID.
     *
     * @param p_mem_id The ID to get the data from.
     * @return The pointer to the data to get.
     */
    virtual void* get_data(MemID p_mem_id) = 0;

    /**
     * @brief Get the data at ID.
     *
     * @param p_mem_id The ID to get the data from.
     * @return The pointer to the data to get.
     */
    virtual const void* get_data(MemID p_mem_id) const = 0;

    /**
     * @brief Get all resource keys in the chunk.
     */
    virtual std::vector<std::string> get_all_keys() const = 0;

    /**
     * @brief Check if the chunk contains a resource with the given key.
     *
     * @param p_res_key The key to check.
     * @return True if the chunk contains the resource, false otherwise.
     */
    virtual bool contains(const std::string& p_res_key) const = 0;

    /**
     * @brief Create a reference to a resource in the chunk.
     * This reference will be resolved after the resource chunk is loaded.
     *
     * @param p_ref The reference to store the resolved resource.
     * @param p_key The key of the resource to reference.
     */
    template <typename T>
    void create_ref_to(RefRes<T>& p_ref, const std::string& p_key) {
        create_ref_to_resolver(
            [&p_ref](RefRes<char> p_resource) {
                p_ref = p_resource.template force_cast<T>();
            },
            p_key);
    }

    /**
     * @brief Push a resource unit to the chunk.
     *
     * @tparam T The type of the resource unit.
     * @param p_id The identifier of the resource unit.
     * @param p_allocator The allocator to allocate memory.
     * @param p_res The resource unit instance.
     * @return The memory id of the pushed resource unit.
     */
    template <ResourceUnitConcept T>
    RefRes<char> push_res_unit(const std::string& p_id, const T& p_res) {
        return push_res_unit_ptr(p_id, &p_res, sizeof(T));
    }

    /**
     * @brief Get the data at ID.
     *
     * @tparam T the type of the data to get.
     * @param p_mem_id The ID to get the data from.
     * @return The pointer to the data to get.
     */
    template <typename T>
    T* get_data(MemID p_mem_id) {
        return static_cast<T*>(get_data(p_mem_id));
    }

    /**
     * @brief Get the data at ID.
     *
     * @tparam T the type of the data to get.
     * @param p_mem_id The ID to get the data from.
     * @return The pointer to the data to get.
     */
    template <typename T>
    const T* get_data(MemID p_mem_id) const {
        return static_cast<const T*>(get_data(p_mem_id));
    }

protected:
    /**
     * @brief Create a type-erased reference resolver.
     *
     * @param p_assign_ref The callback that writes the resolved resource.
     * @param p_key The key of the resource to reference.
     */
    virtual void create_ref_to_resolver(std::function<void(RefRes<char>)> p_assign_ref, const std::string& p_key) = 0;

    /**
     * @brief Push the resource unit pointer to the chunk.
     *
     * @param p_res_key The key of the resource unit.
     * @param p_res The pointer to the resource unit.
     * @param p_size The size of the resource unit.
     */
    virtual RefRes<char> push_res_unit_ptr(const std::string& p_res_key, const void* p_res, size_t p_size) = 0;
};

} // namespace WhiteBirdEngine

#endif
