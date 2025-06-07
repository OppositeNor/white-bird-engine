#ifndef __WBE_REFERENCE_STRONG_H__
#define __WBE_REFERENCE_STRONG_H__

#include "core/allocator/heap_allocator.h"
#include "utils/defs.h"
#include <atomic>

namespace WhiteBirdEngine {

template <typename T>
class RefWeak;

// TODO: Test multi-thread
/**
 * @class
 * @brief The reference to a memory resource.
 *
 * @tparam T The type of the resource.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType = HeapAllocator>
class Ref {
    friend class RefWeak<T>;
    struct ControlBlock;
public:

    using ObjType = T;
    using HeapAllocatorType = AllocType;

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
        p_other.control_block = nullptr;
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
        control_block = p_other.control_block;
        p_other.control_block = nullptr;
        return *this;
    }

    /**
     * @brief Create a reference with a given allocator and memory ID.
     *
     * @param p_allocator The allocator that allocated the memory for memory ID.
     * @param p_mem_id The memory ID.
     */
    Ref(AllocType* p_allocator, MemID p_mem_id) {
        WBE_DEBUG_ASSERT(p_allocator != nullptr);
        control_block = new ControlBlock(p_allocator, p_mem_id);
    }

    /**
     * @brief Make a reference with given constructor arguments.
     *
     * @tparam Args The argument types of the constructor.
     * @param p_allocator The allocator to allocate the object.
     * @param p_args The arguments of the constructor.
     * @return The created reference.
     */
    template <typename... Args>
    static Ref<T> make_ref(AllocType* p_allocator, Args&&... p_args) {
        return Ref(p_allocator, create_obj<T>(*p_allocator, std::forward<Args>(p_args)...));
    }

    T* operator->() {
        return control_block->allocator->get(control_block->mem_id);
    }

    T& operator*() {
        return *static_cast<T*>(control_block->allocator->get(control_block->mem_id));
    }

    const T& operator*() const {
        return *static_cast<const T*>(control_block->allocator->get(control_block->mem_id));
    }

private:

    Ref(ControlBlock* p_control_block)
        : control_block(p_control_block) {
        ref();
    }

    struct ControlBlock {
        ControlBlock(AllocType* p_alloc_type, MemID p_mem_id)
            : mem_id(p_mem_id), allocator(p_alloc_type) {
            weak_ref_counter.store(0, std::memory_order_release);
            strong_ref_counter.store(1, std::memory_order_release);
        }
        MemID mem_id;
        AllocType* allocator;
        std::atomic<uint32_t> weak_ref_counter;
        std::atomic<uint32_t> strong_ref_counter;
    };

    void ref() const {
        if (control_block == nullptr) {
            return;
        }
        control_block->strong_ref_counter.fetch_add(1, std::memory_order_acq_rel);
    }

    void deref() const {
        if (control_block == nullptr) {
            return;
        }
        if (control_block->strong_ref_counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            destroy_obj<T>(*(control_block->allocator), control_block->mem_id);
            if (control_block->weak_ref_counter.load(std::memory_order_acquire) == 0) {
                delete control_block;
            }
        }
    }

    mutable ControlBlock* control_block;
};

template <typename T, typename AllocType,  typename... Args>
Ref<T, AllocType> make_ref(AllocType* p_allocator, Args&&... p_args) {
    WBE_DEBUG_ASSERT(p_allocator != nullptr);
    MemID id = create_obj(*p_allocator, std::forward<Args>(p_args)...);
    return Ref<T, AllocType>(p_allocator, id);
}

}

#endif
