#ifndef __COMPONENTS_H__
#define __COMPONENTS_H__

#include "utils/allocator/allocator_pool.h"
#include "utils/defs.h"
#include "utils/utils.h"
#include <cstddef>

namespace WhiteBirdEngine {

template <typename SysType, typename CompType>
constexpr bool is_system_of = requires(SysType p_t) {
    p_t.operate((CompType*)nullptr);
};

// TODO: test
template <typename T>
class ComponentsChunk {
public:
    ComponentsChunk(size_t p_pool_size, size_t p_align = alignof(std::max_align_t))
        : component_pool(p_pool_size, p_align) {}

    template <typename ...Args>
    MemID add(Args&&... p_args) {
        return create_obj(component_pool, std::forward(p_args)...);
    }
    void remove(MemID p_obj) {
        destroy_obj<T>(component_pool, p_obj);
    }

    template <typename T1>
    void system_operate(const T1& p_system) {
        WBE_DEBUG_ASSERT(component_pool.is_flushed());
        T* mem_ptr = static_cast<T*>(component_pool.get_mem_start());
        uint32_t obj_count = component_pool.obj_count();
        size_t aligned_size = get_align_size(sizeof(T), component_pool.get_alignment());
        while (obj_count > 0) {
            p_system(mem_ptr);
            --obj_count;
            mem_ptr = reinterpret_cast<T*>(reinterpret_cast<std::byte*>(mem_ptr) + aligned_size);
        }
    }

private:
    AllocatorPool component_pool;
};

}


#endif
