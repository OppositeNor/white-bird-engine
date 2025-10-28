/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef __WBE_REFERENCE_STRONG_HH__
#define __WBE_REFERENCE_STRONG_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include "utils/defs.hh"
#include <atomic>
#include <cstddef>
#include <functional>
#include <stdexcept>

namespace WhiteBirdEngine {


/**
 * @class Ref
 * @brief The reference to a memory resource.
 *
 * @tparam T The type of the resource.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType = HeapAllocatorAligned>
class Ref {
    template <typename T1, typename AllocType1>
    friend class Ref;

    template <typename T1, typename AllocTYpe1>
    friend class RefWeak;

    friend struct ::std::hash<Ref<T, AllocType>>;

    struct ControlBlock;
public:

    using ObjType = T;

    Ref() {
        control_block = nullptr;
        ref();
    }
    ~Ref() {
        deref();
    }
    Ref(const Ref& p_other) {
        p_other.ref();
        control_block = p_other.control_block;
    }
    Ref(Ref&& p_other) {
        control_block = p_other.control_block;
        p_other.control_block = nullptr;
    }
    Ref& operator=(const Ref& p_other) {
        if (*this == p_other) {
            return *this;
        }
        deref();
        p_other.ref();
        control_block = p_other.control_block;
        return *this;
    }
    Ref& operator=(Ref&& p_other) {
        if (*this == p_other) {
            return *this;
        }
        deref();
        control_block = p_other.control_block;
        p_other.control_block = nullptr;
        return *this;
    }

    Ref(MemID p_mem_id) {
        if (p_mem_id == MEM_NULL) {
            control_block = nullptr;
            return;
        }
        throw std::runtime_error("Cannot directly asign an memory id to a reference unless it's MEM_NULL.");
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Ref(const Ref<T1, AllocType1>& p_other) {
        p_other.ref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Ref(Ref<T1, AllocType1>&& p_other) {
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        p_other.control_block = nullptr;
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Ref& operator=(const Ref<T1, AllocType1>& p_other) {
        if (*this == p_other) {
            return *this;
        }
        deref();
        p_other.ref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        return *this;
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Ref& operator=(Ref<T1, AllocType1>&& p_other) {
        if (*this == p_other) {
            return *this;
        }
        deref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        p_other.control_block = nullptr;
        return *this;
    }

    Ref& operator=(std::nullptr_t) {
        deref();
        control_block = nullptr;
        return *this;
    }

    Ref& operator=(void* p_ptr) {
        WBE_DEBUG_ASSERT(p_ptr == nullptr);
        deref();
        control_block = nullptr;
        return *this;
    }

    Ref& operator=(MemID p_mem_id) {
        WBE_DEBUG_ASSERT(p_mem_id == MEM_NULL);
        deref();
        control_block = nullptr;
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
        MemID control_block_mem_id = create_obj_align<ControlBlock>(*(p_allocator), p_allocator, p_mem_id);
        control_block = p_allocator->template get_obj<ControlBlock>(control_block_mem_id);
        control_block->control_block_mem_id = control_block_mem_id;
        ref();
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
    static Ref<T, AllocType> make_ref(AllocType* p_allocator, Args&&... p_args) {
        return Ref(p_allocator, create_obj_align<T>(*p_allocator, std::forward<Args>(p_args)...));
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
        if (control_block == nullptr) {
            return nullptr;
        }
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return static_cast<T*>(control_block->allocator->get(control_block->mem_id));
    }

    /**
     * @brief Get the resource pointer.
     *
     * @return The pointer pointing to the resource. nullptr if mem_id is MEM_NULL.
     */
    const T* get() const {
        if (control_block == nullptr) {
            return nullptr;
        }
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return static_cast<T*>(control_block->allocator->get(control_block->mem_id));
    }

    /**
     * @brief Dynamic cast to a different type of reference.
     *
     * @todo Test
     * @tparam T1 The type to cast to.
     * @return The casted reference. MEM_NULL if the cast fails.
     */
    template <typename T1>
    Ref<T1, AllocType> dynamic_cast_ref() const {
        if (control_block == nullptr) {
            return Ref<T1, AllocType>(nullptr);
        }
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        T1* casted_ptr = dynamic_cast<T1*>(static_cast<T*>(control_block->allocator->get(control_block->mem_id)));
        if (casted_ptr == nullptr) {
            return MEM_NULL;
        }
        return Ref<T1, AllocType>(reinterpret_cast<typename Ref<T1, AllocType>::ControlBlock*>(control_block));
    }


    template <typename T1, typename AllocType1>
    bool operator==(const Ref<T1, AllocType1>& p_other) const {
        return control_block == reinterpret_cast<decltype(control_block)>(p_other.control_block);
    }

    bool operator==(std::nullptr_t) const {
        return is_null();
    }

    bool operator==(void* p_ptr) const {
        if (p_ptr != nullptr) {
            throw std::runtime_error("Cannot compare a unique with a pointer that is not nullptr.");
        }
        return is_null();
    }

    bool operator==(MemID p_mem_id) const {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Cannot compare a unique with a memory ID that is not MEM_NULL.");
        }
        return is_null();
    }

    template <typename T1>
    bool operator!=(T1 p_obj) const {
        return !(*this == p_obj);
    }

    /**
     * @brief Is the reference NULL.
     *
     * @return true if the reference is NULL, false otherwise.
     */
    bool is_null() const {
        return control_block == nullptr || control_block->mem_id == MEM_NULL;
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
            strong_ref_counter.store(0, std::memory_order_release);
        }
        MemID control_block_mem_id;
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
                destroy_obj<ControlBlock>(*(control_block->allocator), control_block->control_block_mem_id);
            }
            control_block = nullptr;
        }
    }

    mutable ControlBlock* control_block;
};

/**
 * @brief Make a reference with given constructor arguments.
 *
 * @tparam T The type that is created.
 * @tparam Args The argument types of the constructor.
 * @param p_allocator The allocator to allocate the object.
 * @param p_args The arguments of the constructor.
 * @return The created reference.
 */
template <typename T, typename AllocType = HeapAllocatorAligned, typename... Args>
Ref<T, AllocType> make_ref(AllocType* p_allocator, Args&&... p_args) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Allocator cannot be nullptr.");
    }
    MemID id = create_obj_align<T>(*p_allocator, std::forward<Args>(p_args)...);
    return Ref<T, AllocType>(p_allocator, id);
}

}

namespace std {
/**
 * @brief Hash function for reference.
 *
 * @tparam T The type of the reference.
 * @param p_ref The reference to hash.
 * @return 
 */
template <typename T, typename AllocType>
struct hash<::WhiteBirdEngine::Ref<T, AllocType>> {
    size_t operator()(const ::WhiteBirdEngine::Ref<T, AllocType>& p_ref) const {
        if (p_ref.is_null()) {
            return WhiteBirdEngine::MEM_NULL;
        }
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        return std::hash<AllocType*>{}(p_ref.control_block->allocator) ^ std::hash<::WhiteBirdEngine::MemID>{}(p_ref.control_block->mem_id);
    }

};
}

#endif
