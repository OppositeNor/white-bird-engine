#ifndef __WBE_REFERENCE_STRONG_H__
#define __WBE_REFERENCE_STRONG_H__

#include "core/allocator/heap_allocator.h"
#include "core/global/global.h"
#include "utils/defs.h"
#include <atomic>

namespace WhiteBirdEngine {


/**
 * @class
 * @brief The reference to a memory resource.
 *
 * @tparam T The type of the resource.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType = HeapAllocator>
class Ref {
    template <typename T1, typename AllocTyp1>
    friend class Ref;

    template <typename T1>
    friend class RefWeak;

    struct ControlBlock;
public:

    using ObjType = T;
    using HeapAllocatorType = AllocType;

    explicit Ref(AllocType* p_allocator = Global::get_singleton()->pool_allocator.get()) {
        WBE_DEBUG_ASSERT(p_allocator != nullptr);
        control_block = new ControlBlock(p_allocator, MEM_NULL);
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

    template <typename T1>
    Ref(const Ref<T1>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        p_other.ref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
    }
    template <typename T1>
    Ref(Ref<T1>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        p_other.control_block = nullptr;
    }
    template <typename T1>
    Ref& operator=(const Ref<T1>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        deref();
        p_other.ref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        return *this;
    }
    template <typename T1>
    Ref& operator=(Ref<T1>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        deref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        p_other.control_block = nullptr;
        return *this;
    }

    bool is_valid() const {
        return control_block != nullptr;
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
        WBE_DEBUG_ASSERT(control_block != nullptr);
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return static_cast<T*>(control_block->allocator->get(control_block->mem_id));
    }

    const T* operator->() const {
        WBE_DEBUG_ASSERT(control_block != nullptr);
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return static_cast<T*>(control_block->allocator->get(control_block->mem_id));
    }

    T& operator*() {
        WBE_DEBUG_ASSERT(control_block != nullptr);
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return *static_cast<T*>(control_block->allocator->get(control_block->mem_id));
    }

    const T& operator*() const {
        WBE_DEBUG_ASSERT(control_block != nullptr);
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return *static_cast<const T*>(control_block->allocator->get(control_block->mem_id));
    }

    /**
     * @brief Get the resource pointer.
     *
     * @return The pointer pointing to the resource. nullptr if mem_id is MEM_NULL.
     */
    T* get() {
        WBE_DEBUG_ASSERT(control_block != nullptr);
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return static_cast<T*>(control_block->allocator->get(control_block->mem_id));
    }

    /**
     * @brief Get the resource pointer.
     *
     * @return The pointer pointing to the resource. nullptr if mem_id is MEM_NULL.
     */
    const T* get() const {
        WBE_DEBUG_ASSERT(control_block != nullptr);
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return static_cast<T*>(control_block->allocator->get(control_block->mem_id));
    }

    bool operator==(const Ref<T>& p_other) {
        return p_other.control_block == control_block;
    }

private:

    Ref(ControlBlock* p_control_block)
        : control_block(p_control_block) {
        WBE_DEBUG_ASSERT(p_control_block != nullptr);
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
            control_block = nullptr;
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
