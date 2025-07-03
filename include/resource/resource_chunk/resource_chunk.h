#ifndef __WBE_RESOURCE_CHUNK_H__
#define __WBE_RESOURCE_CHUNK_H__

#include "core/allocator/allocator.h"
#include "core/allocator/stack_allocator.h"
#include "core/parser/parser.h"
#include <cstddef>
#include <cstring>
#include <string>
#include <unordered_map>
namespace WhiteBirdEngine {

class ResourceManager;

/**
 * @class ResourceChunk
 * @brief Chunk of resources. Used for containing multiple resources to be loaded and deloaded at once.
 * @todo Test
 */
class ResourceChunk {
public:
    virtual ~ResourceChunk() {}
    ResourceChunk(const ResourceChunk&) = delete;
    ResourceChunk(ResourceChunk&&) = delete;
    ResourceChunk& operator=(const ResourceChunk&) = delete;
    ResourceChunk& operator=(ResourceChunk&&) = delete;

    /**
     * @brief Get the resource from a parser.
     *
     * @param p_parser The parser to get resource from.
     */
    template <ParserConcept ParserType>
    ResourceChunk(const ParserType& p_parser) {
        // TODO
    }

    /**
     * @brief Get the resource of a specific key.
     *
     * @param p_key The key to get resource from.
     * @return The MemID of the resource.
     */
    virtual MemID get_resource(const std::string& p_key) const {
        return resource_map.at(p_key);
    }

    /**
     * @brief Get the total size of the chunk.
     *
     * @return The total size of the chunk.
     */
    size_t get_size() const {
        return total_size;
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
    template <typename T>
    MemID push_res_unit(const std::string& p_res_key, StackAllocator& p_allocator, const T& p_res) {
        using PureT = std::remove_cvref<T>;
        total_size += sizeof(PureT);
        MemID result = p_allocator.allocate(sizeof(PureT));
        *(p_allocator.get_obj<PureT>(result)) = p_res;
        resource_map[p_res_key] = result;
        return result;
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
        total_size += p_res_size;
        MemID result = p_allocator.allocate(p_res_size);
        memcpy(p_allocator.get(result), p_res, p_res_size);
        return result;
    }

private:
    size_t total_size;
    std::unordered_map<std::string, MemID> resource_map;
};

}

#endif
