#ifndef __WBE_ECS_H__
#define __WBE_ECS_H__

#include "utils/allocator/allocator_fixed_size_pool.h"
#include "utils/defs.h"
#include <atomic>
#include <cstddef>

namespace WhiteBirdEngine {

using EntityID = uint64_t;
using ComponentTypeID = uint64_t;

inline std::atomic<ComponentTypeID> next_component_id = 0;

template <typename SysType, typename CompType>
constexpr bool is_system_of = requires(SysType p_t) {
    p_t.operate((CompType*)nullptr);
};

// TODO: test
template <typename T>
class ComponentsChunk {
public:
    using ComponentType = T;

    ComponentsChunk(size_t p_pool_size)
        : component_pool(p_pool_size, sizeof(T)) {}

    template <typename ...Args>
    MemID add(EntityID p_entity_id, Args&&... p_args) {
        MemID mem_id = create_obj(component_pool, std::forward(p_args)...);
        entity_map[p_entity_id] = mem_id;
        return mem_id;
    }
    void remove(EntityID p_entity_id, MemID p_obj) {
        entity_map.erase(p_entity_id);
        destroy_obj<T>(component_pool, p_obj);
    }

    template <typename T1>
    void system_operate(const T1& p_system) {
        WBE_DEBUG_ASSERT(component_pool.is_flushed());
        T* mem_array = static_cast<T*>(component_pool.get_mem_start());
        uint32_t obj_count = component_pool.obj_count();
        for (uint32_t i = 0; i < obj_count; ++i) {
            p_system(mem_array + i);
        }
    }

    MemID get(EntityID p_entity) {
        auto comp_iter = entity_map.find(p_entity);
        if (comp_iter == entity_map.end()) {
            throw std::runtime_error("Failed to get component from entity with id: " + std::to_string(p_entity));
        }
    }

private:
    AllocatorFixedSizePool<sizeof(T)> component_pool;
    std::unordered_map<EntityID, MemID> entity_map;
};

}

#endif
