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
#ifndef WBE_FILE_REFERENCE_STRONG_HH
#define WBE_FILE_REFERENCE_STRONG_HH

#include "core/allocator/heap_allocator.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/subobject_offset.hh"
#include "utils/defs.hh"
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>

namespace WhiteBirdEngine {

/**
 * @struct RefControlBlock
 * @brief The shared state of the Ref and RefWeak handles of one object.
 *
 * Every strong reference also holds a hidden weak reference, so the block is destroyed by the last reference of either
 * kind. The block does not depend on the viewed type, so handles of any type converted from one object share it.
 *
 * @tparam AllocType The type of the allocator.
 */
template <typename AllocType>
struct RefControlBlock {
    /**
     * @brief Destroys the object and frees its allocation. Captured with the type the object was created as, so the
     * last reference destroys that type whatever type it views.
     */
    using DestroyObjectFunction = void (*)(void* p_allocator, MemID p_mem_id);

    RefControlBlock(AllocType* p_allocator, MemID p_mem_id, DestroyObjectFunction p_destroy_object)
        : mem_id(p_mem_id), allocator(p_allocator), destroy_object(p_destroy_object) {
        weak_ref_counter.store(0, std::memory_order_release);
        strong_ref_counter.store(0, std::memory_order_release);
    }

    /**
     * @brief The memory ID of this control block.
     */
    MemID control_block_mem_id = MEM_NULL;
    /**
     * @brief The memory ID of the whole object allocation.
     */
    MemID mem_id;
    /**
     * @brief The allocator of both the object and this control block.
     */
    AllocType* allocator;
    /**
     * @brief Destroys the object as the type it was created as.
     */
    DestroyObjectFunction destroy_object;
    /**
     * @brief The number of weak references, including one hidden weak reference per strong reference.
     */
    std::atomic<uint64_t> weak_ref_counter;
    /**
     * @brief The number of strong references.
     */
    std::atomic<uint64_t> strong_ref_counter;
};

/**
 * @class Ref
 * @brief The reference to a memory resource.
 *
 * Copyable and movable. A moved-from reference is null. A reference converted to a base type views that base
 * subobject, and the last reference destroys the object as the type it was created as.
 *
 * @tparam T The type of the resource.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType = HeapAllocator>
class Ref {
    template <typename T1, typename AllocType1>
    friend class Ref;

    template <typename T1, typename AllocTYpe1>
    friend class RefWeak;

    friend struct ::std::hash<Ref<T, AllocType>>;

    using ControlBlock = RefControlBlock<AllocType>;

public:
    using ObjType = T;

    Ref() : control_block(nullptr) {
    }
    ~Ref() {
        deref();
    }
    Ref(const Ref& p_other) : control_block(p_other.control_block), object_offset(p_other.object_offset) {
        ref();
    }
    Ref(Ref&& p_other) noexcept : control_block(p_other.control_block), object_offset(p_other.object_offset) {
        p_other.control_block = nullptr;
    }
    Ref& operator=(const Ref& p_other) {
        if (this == &p_other) {
            return *this;
        }
        p_other.ref();
        deref();
        control_block = p_other.control_block;
        object_offset = p_other.object_offset;
        return *this;
    }
    Ref& operator=(Ref&& p_other) noexcept {
        if (this == &p_other) {
            return *this;
        }
        // Safe when both share the control block: p_other still holds its strong reference.
        deref();
        control_block = p_other.control_block;
        object_offset = p_other.object_offset;
        p_other.control_block = nullptr;
        return *this;
    }

    Ref(MemID p_mem_id) : control_block(nullptr) {
        if (p_mem_id == MEM_NULL) {
            return;
        }
        throw std::runtime_error("Cannot directly asign an memory id to a "
                                 "reference unless it's MEM_NULL.");
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Ref(const Ref<T1, AllocType1>& p_other)
        : control_block(reinterpret_cast<ControlBlock*>(p_other.control_block)), object_offset(get_converted_offset(p_other)) {
        ref();
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Ref(Ref<T1, AllocType1>&& p_other) noexcept
        : control_block(reinterpret_cast<ControlBlock*>(p_other.control_block)), object_offset(get_converted_offset(p_other)) {
        p_other.control_block = nullptr;
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Ref& operator=(const Ref<T1, AllocType1>& p_other) {
        std::ptrdiff_t offset = get_converted_offset(p_other);
        p_other.ref();
        deref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        object_offset = offset;
        return *this;
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Ref& operator=(Ref<T1, AllocType1>&& p_other) noexcept {
        std::ptrdiff_t offset = get_converted_offset(p_other);
        deref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        object_offset = offset;
        p_other.control_block = nullptr;
        return *this;
    }

    Ref& operator=(std::nullptr_t) {
        deref();
        return *this;
    }

    /**
     * @brief Reset the reference.
     *
     * @param p_ptr Must be nullptr.
     * @throws std::runtime_error If p_ptr is not nullptr.
     */
    Ref& operator=(void* p_ptr) {
        if (p_ptr != nullptr) {
            throw std::runtime_error("Cannot assign a pointer that is not nullptr to a reference.");
        }
        deref();
        return *this;
    }

    /**
     * @brief Reset the reference.
     *
     * @param p_mem_id Must be MEM_NULL.
     * @throws std::runtime_error If p_mem_id is not MEM_NULL.
     */
    Ref& operator=(MemID p_mem_id) {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Cannot assign a memory ID that is not MEM_NULL to a reference.");
        }
        deref();
        return *this;
    }

    /**
     * @brief Create a reference with a given allocator and memory ID.
     *
     * The object is destroyed as T when the last reference is released.
     *
     * @param p_allocator The allocator that allocated the memory for memory ID.
     * @param p_mem_id The memory ID.
     */
    Ref(AllocType* p_allocator, MemID p_mem_id) {
        WBE_DEBUG_ASSERT(p_allocator != nullptr);
        MemID control_block_mem_id = create_obj<ControlBlock>(*p_allocator, p_allocator, p_mem_id, &destroy_created_object);
        control_block = p_allocator->template get_obj<ControlBlock>(control_block_mem_id);
        control_block->control_block_mem_id = control_block_mem_id;
        ref();
    }

    /**
     * @brief Make a reference with given constructor arguments. Same as the free make_ref().
     *
     * @tparam Args The argument types of the constructor.
     * @param p_allocator The allocator to allocate the object.
     * @param p_args The arguments of the constructor.
     * @return The created reference.
     * @throws std::runtime_error If p_allocator is nullptr.
     */
    template <typename... Args>
    static Ref<T, AllocType> make_ref(AllocType* p_allocator, Args&&... p_args);

    /**
     * @brief Get the resource pointer.
     *
     * @return The pointer pointing to the resource. nullptr if mem_id is
     * MEM_NULL.
     */
    T* get() {
        if (is_null()) {
            return nullptr;
        }
        return get_object();
    }

    /**
     * @brief Get the resource pointer.
     *
     * @return The pointer pointing to the resource. nullptr if mem_id is
     * MEM_NULL.
     */
    const T* get() const {
        if (is_null()) {
            return nullptr;
        }
        return get_object();
    }

    /**
     * @brief Dynamic cast to a different type of reference.
     *
     * @tparam T1 The type to cast to.
     * @return The casted reference, sharing ownership with this one and viewing the T1 subobject. MEM_NULL if the cast
     * fails.
     */
    template <typename T1>
    Ref<T1, AllocType> dynamic_cast_ref() const {
        if (is_null()) {
            return Ref<T1, AllocType>();
        }
        T1* casted_ptr = dynamic_cast<T1*>(get_object());
        if (casted_ptr == nullptr) {
            return Ref<T1, AllocType>();
        }
        void* allocation = control_block->allocator->get(control_block->mem_id);
        return Ref<T1, AllocType>(control_block, get_address_offset(allocation, casted_ptr));
    }

    /**
     * @brief Is the reference NULL.
     *
     * @return true if the reference is NULL, false otherwise.
     */
    bool is_null() const {
        return control_block == nullptr || control_block->mem_id == MEM_NULL;
    }

    T* operator->() {
        return get_object();
    }

    const T* operator->() const {
        return get_object();
    }

    T& operator*() {
        return *get_object();
    }

    const T& operator*() const {
        return *get_object();
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
            throw std::runtime_error("Cannot compare a unique with a memory ID "
                                     "that is not MEM_NULL.");
        }
        return is_null();
    }

    template <typename T1>
    bool operator!=(T1 p_obj) const {
        return !(*this == p_obj);
    }

    /**
     * @brief Order references by their control block. Null references order first and equal to each other.
     *
     * @return Negative, zero, or positive.
     */
    template <typename T1, typename AllocType1>
    int operator<=>(const Ref<T1, AllocType1>& p_obj) const {
        const void* self = is_null() ? nullptr : static_cast<const void*>(control_block);
        const void* other = p_obj.is_null() ? nullptr : static_cast<const void*>(p_obj.control_block);
        if (std::less<const void*>{}(self, other)) {
            return -1;
        }
        if (std::less<const void*>{}(other, self)) {
            return 1;
        }
        return 0;
    }

private:
    Ref(ControlBlock* p_control_block, std::ptrdiff_t p_object_offset)
        : control_block(p_control_block), object_offset(p_object_offset) {
        ref();
    }

    /**
     * @brief Acquire a strong reference unless the object has already been destroyed.
     *
     * The strong count is only incremented while it is nonzero, so a reference that is being released concurrently
     * cannot be revived.
     */
    static Ref acquire_from_weak(ControlBlock* p_control_block, std::ptrdiff_t p_object_offset) {
        Ref result;
        if (p_control_block == nullptr) {
            return result;
        }
        uint64_t strong_count = p_control_block->strong_ref_counter.load(std::memory_order_acquire);
        while (strong_count != 0) {
            if (p_control_block->strong_ref_counter.compare_exchange_weak(
                    strong_count, strong_count + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
                // The caller's weak reference keeps the control block alive until the hidden weak reference is added.
                p_control_block->weak_ref_counter.fetch_add(1, std::memory_order_relaxed);
                result.control_block = p_control_block;
                result.object_offset = p_object_offset;
                return result;
            }
        }
        return result;
    }

    template <typename T1, typename AllocType1>
    static std::ptrdiff_t get_converted_offset(const Ref<T1, AllocType1>& p_other) {
        if (p_other.is_null()) {
            return 0;
        }
        return get_converted_subobject_offset<T>(p_other.get_object(), p_other.object_offset);
    }

    static void destroy_created_object(void* p_allocator, MemID p_mem_id) {
        destroy_obj<T>(*static_cast<AllocType*>(p_allocator), p_mem_id);
    }

    T* get_object() const {
        WBE_DEBUG_ASSERT(control_block != nullptr);
        WBE_DEBUG_ASSERT(control_block->allocator != nullptr);
        return apply_subobject_offset<T>(control_block->allocator->get(control_block->mem_id), object_offset);
    }

    void ref() const {
        if (control_block == nullptr) {
            return;
        }
        control_block->strong_ref_counter.fetch_add(1, std::memory_order_relaxed);
        // For all strong reference, create a hidden weak reference, since only
        // weak reference could destruct control block.
        control_block->weak_ref_counter.fetch_add(1, std::memory_order_relaxed);
    }

    void deref() const {
        if (control_block == nullptr) {
            return;
        }
        if (control_block->strong_ref_counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // If all the references of the control block is freed, destroy the
            // object.
            control_block->destroy_object(control_block->allocator, control_block->mem_id);
        }
        // Dereference the hidden weak reference.
        weak_deref();
    }

    void weak_deref() const {
        uint64_t weak_ref_count = control_block->weak_ref_counter.fetch_sub(1, std::memory_order_acq_rel);
        if (weak_ref_count == 1) {
            // Every strong reference holds a hidden weak reference, so the last weak reference is also past the last
            // strong reference: destroy the control block.
            destroy_obj<ControlBlock>(*(control_block->allocator), control_block->control_block_mem_id);
        }
        control_block = nullptr;
    }

    mutable ControlBlock* control_block;
    std::ptrdiff_t object_offset = 0;
};

template <typename T>
concept ThisRefAsignable = requires(T* p_ins, Ref<T> p_ref) { p_ins->set_ref_of_this(p_ref); };

/**
 * @brief Make a reference with given constructor arguments.
 *
 * @tparam T The type that is created.
 * @tparam Args The argument types of the constructor.
 * @param p_allocator The allocator to allocate the object.
 * @param p_args The arguments of the constructor.
 * @return The created reference.
 * @throws std::runtime_error If p_allocator is nullptr.
 */
template <typename T, typename AllocType = HeapAllocator, typename... Args>
Ref<T, AllocType> make_ref(AllocType* p_allocator, Args&&... p_args) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Allocator cannot be nullptr.");
    }
    MemID id = create_obj<T>(*p_allocator, std::forward<Args>(p_args)...);
    Ref<T, AllocType> result(p_allocator, id);
    if constexpr (ThisRefAsignable<T>) {
        result->set_ref_of_this(result);
    }
    return result;
}

template <typename T, typename AllocType>
template <typename... Args>
Ref<T, AllocType> Ref<T, AllocType>::make_ref(AllocType* p_allocator, Args&&... p_args) {
    return WhiteBirdEngine::make_ref<T, AllocType>(p_allocator, std::forward<Args>(p_args)...);
}

} // namespace WhiteBirdEngine

namespace std {
/**
 * @brief Hash function for reference.
 *
 * @tparam T The type of the reference.
 * @param p_ref The reference to hash.
 * @return
 */
template <typename T, typename AllocType>
struct hash<::WhiteBirdEngine::Ref<T, AllocType>> { // NOLINT
    size_t operator()(const ::WhiteBirdEngine::Ref<T, AllocType>& p_ref) const {
        if (p_ref.is_null()) {
            return WhiteBirdEngine::MEM_NULL;
        }
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        return std::hash<AllocType*>{}(p_ref.control_block->allocator) ^
               std::hash<::WhiteBirdEngine::MemID>{}(p_ref.control_block->mem_id);
    }
};
} // namespace std

#endif
