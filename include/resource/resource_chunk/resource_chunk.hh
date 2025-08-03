#ifndef __WBE_RESOURCE_CHUNK_H__
#define __WBE_RESOURCE_CHUNK_H__

#include "core/allocator/allocator.h"
#include "core/allocator/stack_allocator.h"
#include "resource/resource_unit/resource_unit.h"
#include "utils/defs.h"
#include "utils/utils.h"

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
    MemID push_res_unit(const std::string& p_res_key, StackAllocator& p_allocator, const T& p_res) {
        return static_cast<ChildT*>(this)->push_res_unit(p_res_key, p_allocator, p_res);
    }

    /**
     * @brief Push a chunk of data into the chunk. The data will be copied to the allocator.
     *
     * @param p_res_size The size of the resource to push to.
     * @param p_res The pointer to the resource to be pushed into the chunk.
     * @param p_allocator The allocator to allocate memory.
     * @return The result memory id.
     */
    MemID push_data(size_t p_res_size, void* p_res, StackAllocator& p_allocator) {
        return static_cast<ChildT*>(this)->push_data(p_res_size, p_res, p_allocator);
    }

};

WBE_DECL_CRTP_CONCEPT(ResourceChunk);

}

#endif
