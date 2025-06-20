#ifndef __WBE_ECS_H__
#define __WBE_ECS_H__

#include "core/allocator/heap_allocator_fixed_size_pool.h"
#include "utils/defs.h"

namespace WhiteBirdEngine {

using EntityID = uint64_t;

/**
 * @brief Concept for identifying if a system can operate on a component.
 *
 * @tparam SysType The type of the system.
 * @tparam CompType The type of the component.
 */
template <typename SysType, typename CompType>
concept SystemConcept = requires(SysType p_t) {
    p_t.operate((CompType*)nullptr);
};

template <typename T>
concept ComponentsChunkAllocatorConcept = AllocatorTrait<T>::IS_GURANTEED_CONTINUOUS
    && AllocatorTrait<T>::TYPE == AllocatorType::HEAP_ALLOCATOR
    && AllocatorTrait<T>::IS_ALLOC_FIXED_SIZE;

/**
 * @class ComponentsChunk
 * @brief A chunk of components.
 * @todo test
 *
 * @tparam T The type of the component.
 * @tparam Alloctype The type of the allocator.
 */
template <typename T, ComponentsChunkAllocatorConcept AllocType = HeapAllocatorFixedSizePool<sizeof(T)>>
class ComponentsChunk {
public:
    using ComponentType = T;
    using AllocatorType = AllocType;

    template <typename... Args>
    ComponentsChunk(Args&&... p_args)
        : component_allocator(std::forward<Args>(p_args)...) {}

    /**
     * @brief Add a component to the chunk.
     *
     * @tparam Args Argument types of the constructor of the component.
     * @param p_entity_id The entity that owns this component.
     * @param p_args The arguments of the constructor of the component.
     * @return The memory ID of the component.
     */
    template <typename ...Args>
    MemID add(EntityID p_entity_id, Args&&... p_args) {
        MemID mem_id = create_obj(component_allocator, std::forward(p_args)...);
        entity_map[p_entity_id] = mem_id;
        return mem_id;
    }

    /**
     * @brief Remove a component of an entity from the pool.
     *
     * @param p_entity_id The ID of the entity.
     * @param p_obj The ID of the component.
     * @param p_maybe_not_contained Don't trigger an error if this chunk doesn't contain the component with the corresponding ID.
     */
    void remove(EntityID p_entity_id, MemID p_component, bool p_maybe_not_contained = false) {
        auto iter = entity_map.find(p_entity_id);
        if (iter == entity_map.end() || iter->second != p_entity_id) {
            if (p_maybe_not_contained) {
                throw std::runtime_error("Failed to remove component: component not in this chunk.");
            }
            return;
        }
        entity_map.erase(p_entity_id);
        destroy_obj<T>(component_allocator, p_component);
    }

    /**
     * @brief Operate a system on the components.
     *
     * @tparam T1 The type of the system.
     * @param p_system The system to operate on this concept.
     */
    template <SystemConcept<T> T1>
    void system_operate(const T1& p_system) {
        WBE_DEBUG_ASSERT(component_allocator.is_flushed());
        T* mem_array = static_cast<T*>(component_allocator.get_mem_start());
        uint32_t obj_count = component_allocator.obj_count();
        for (uint32_t i = 0; i < obj_count; ++i) {
            p_system(mem_array + i);
        }
    }

    /**
     * @brief Get a component of an entity.
     *
     * @param p_entity The entity to get component from.
     * @return The ID of the entity. MEM_NULL if not found.
     */
    MemID get(EntityID p_entity) {
        auto comp_iter = entity_map.find(p_entity);
        if (comp_iter == entity_map.end()) {
            return MEM_NULL;
        }
        return comp_iter->second;
    }

private:
    AllocType component_allocator;
    std::unordered_map<EntityID, MemID> entity_map;
};

}

#endif
