#ifndef __WBE_REFERENCE_STRONG_H__
#define __WBE_REFERENCE_STRONG_H__

#include "utils/allocator/allocator.h"
#include "utils/defs.h"
#include <atomic>
namespace WhiteBirdEngine {

// TODO: Test
template <typename T, typename AllocType = Allocator>
class Ref {
public:

    using ObjType = T;
    using AllocatorType = AllocType;

    struct ControlBlock {
        ControlBlock(AllocType* p_alloc_type, MemID p_mem_id)
            : mem_id(p_mem_id), allocator(p_alloc_type) {
            weak_ref_counter.store(0, std::memory_order_release);
            strong_ref_counter.store(0, std::memory_order_release);
        }
        MemID mem_id;
        AllocType* allocator;
        std::atomic<uint32_t> weak_ref_counter;
        std::atomic<uint32_t> strong_ref_counter;
    };

    Ref(AllocType* p_allocator) {
        WBE_DEBUG_ASSERT(p_allocator != nullptr);
        control_block = new ControlBlock(p_allocator, 0);
        ref();
    }
    ~Ref() {
        deref();
    }
    Ref(const Ref& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        p_other.ref();
        control_block = p_other.control_block;
    }
    Ref(Ref&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        control_block = p_other.control_block;
    }
    Ref& operator=(const Ref& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        deref();
        p_other.ref();
        control_block = p_other.control_block;
        return *this;
    }
    Ref& operator=(Ref&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        deref();
        p_other.ref();
        control_block = p_other.control_block;
        return *this;
    }

    Ref(AllocType* p_allocator, MemID p_mem_id) {
        WBE_DEBUG_ASSERT(p_allocator != nullptr);
        control_block = new ControlBlock(p_allocator, p_mem_id);
    }

    T* operator->() {
        return control_block->allocator->get(control_block->mem_id);
    }

private:
    void ref() {
        WBE_DEBUG_ASSERT(control_block != nullptr);
        control_block->strong_ref_counter.fetch_add(1, std::memory_order_acq_rel);
    }

    void deref() {
        WBE_DEBUG_ASSERT(control_block != nullptr);
        if (control_block->strong_ref_counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            destroy_obj(*(control_block->allocator), control_block->mem_id);
            if (control_block->weak_ref_counter.load(std::memory_order_acquire) == 0) {
                delete control_block;
            }
        }
    }

    ControlBlock* control_block;
};

template <typename T, typename AllocType,  typename... Args>
Ref<T, AllocType> make_ref(AllocType* p_allocator, Args&&... p_args) {
    WBE_DEBUG_ASSERT(p_allocator != nullptr);
    MemID id = create_obj(*p_allocator, std::forward<Args>(p_args)...);
    return Ref<T, AllocType>(p_allocator, id);
}

}


#endif
