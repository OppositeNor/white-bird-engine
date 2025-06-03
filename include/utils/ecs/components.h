#ifndef __COMPONENTS_H__
#define __COMPONENTS_H__

#include "utils/allocator/allocator_pool.h"
#include <cstddef>

namespace WhiteBirdEngine {

template <typename SysType, typename CompType>
constexpr bool is_system_of = requires(SysType p_t) {
    p_t.operate((CompType*)nullptr);
};

// TODO: test
template <typename T>
class Components {
public:
    Components(size_t p_pool_size, size_t p_align = alignof(std::max_align_t))
        : component_pool(p_pool_size, p_align) {}

    template <typename ...Args>
    void* add(Args&&... p_args) {
        return create_obj(component_pool, std::forward(p_args)...);
    }
    void remove(void* p_obj) {
        destroy_obj(component_pool, p_obj);
    }

    template <typename T1>
    void system_operate(const T1& p_system) {
        auto& components = component_pool.get_allocated();
        for (void* component : components) {
            p_system.operate(static_cast<T*>(component));
        }
    }

private:
    AllocatorPool component_pool;
};

}


#endif
