#ifndef __WBE_RESOURCE_MANAGER_H__
#define __WBE_RESOURCE_MANAGER_H__

#include "core/allocator/allocator.h"
#include "core/allocator/stack_allocator.h"
#include "core/memory/reference_strong.h"
#include "resource/resource_chunk/resource_chunk.h"
#include "resource/resource_unit/resource_unit.h"
#include "utils/interface/singleton.h"
#include <cstddef>
namespace WhiteBirdEngine {

/**
 * @class ResourceManager
 * @brief Manages the resources loaded and used by the engine.
 *
 * @todo Doc, Test
 */
class ResourceManager : public Singleton<ResourceManager> {
public:
    virtual ~ResourceManager() override {}

    ResourceManager(size_t p_alloc_size)
        : stack_allocator(p_alloc_size) {}

    /**
     * @brief Register a resource chunk.
     *
     * @param p_resource_chunk The resource chunk to register.
     */
    void register_chunk(Ref<ResourceChunk> p_resource_chunk) {
        resource_chunks.push_back(p_resource_chunk);
    }

    /**
     * @brief Pop a resource chunk.
     *
     * @param p_resource_chunk The resource chunk to pop out.
     */
    void pop_chunk(Ref<ResourceChunk> p_resource_chunk) {
        size_t pop_size = resource_chunks.back()->get_size();
        resource_chunks.pop_back();
        stack_allocator.pop_stack(pop_size);
    }

    /**
     * @brief Get a resource from a chunk.
     *
     * @param p_chunk The resource chunk to get resource from.
     * @param p_key The key of the resource.
     */
    template <ResourceUnitConcept T>
    T* get_resource(Ref<ResourceChunk> p_chunk, const std::string& p_key) {
        return stack_allocator.get_obj<T>(p_chunk->get_resource(p_key));
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

private:
    std::vector<Ref<ResourceChunk>> resource_chunks;
    StackAllocator stack_allocator;
};

}

#endif
