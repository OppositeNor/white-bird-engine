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
#ifndef WBE_FILE_REFERENCE_WEAK_HH
#define WBE_FILE_REFERENCE_WEAK_HH

#include "core/allocator/heap_allocator.hh"
#include "core/allocator/i_allocator.hh"
#include "reference_strong.hh"
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>

namespace WhiteBirdEngine {

/**
 * @class RefWeak
 * @brief A non-owning observer of an object owned by Ref.
 *
 * Copyable and movable. A moved-from weak reference is null. Locking never revives an object whose last strong
 * reference is being released, so lock() is safe to call while other threads release strong references.
 *
 * @tparam T The type of the resource.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType = HeapAllocator>
class RefWeak {
    template <typename T1, typename AllocType1>
    friend class RefWeak;

    friend struct ::std::hash<RefWeak<T, AllocType>>;

    using ControlBlock = RefControlBlock<AllocType>;

public:
    using ObjType = T;

    RefWeak() : control_block(nullptr) {
    }
    ~RefWeak() {
        deref();
    }
    RefWeak(const RefWeak<T, AllocType>& p_other) : control_block(p_other.control_block), object_offset(p_other.object_offset) {
        ref();
    }
    RefWeak(RefWeak<T, AllocType>&& p_other) noexcept
        : control_block(p_other.control_block), object_offset(p_other.object_offset) {
        p_other.control_block = nullptr;
    }
    RefWeak& operator=(const RefWeak<T, AllocType>& p_other) {
        if (this == &p_other) {
            return *this;
        }
        p_other.ref();
        deref();
        control_block = p_other.control_block;
        object_offset = p_other.object_offset;
        return *this;
    }
    RefWeak& operator=(RefWeak<T, AllocType>&& p_other) noexcept {
        if (this == &p_other) {
            return *this;
        }
        // Safe when both share the control block: p_other still holds its weak reference.
        deref();
        control_block = p_other.control_block;
        object_offset = p_other.object_offset;
        p_other.control_block = nullptr;
        return *this;
    }
    RefWeak(const Ref<T, AllocType>& p_ref) : control_block(p_ref.control_block), object_offset(p_ref.object_offset) {
        ref();
    }
    RefWeak& operator=(const Ref<T, AllocType>& p_ref) {
        ControlBlock* new_control_block = p_ref.control_block;
        increment(new_control_block);
        deref();
        control_block = new_control_block;
        object_offset = p_ref.object_offset;
        return *this;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak(const RefWeak<T1, AllocType1>& p_other)
        : control_block(reinterpret_cast<ControlBlock*>(p_other.control_block)), object_offset(get_converted_offset(p_other)) {
        ref();
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak(RefWeak<T1, AllocType1>&& p_other)
        : control_block(reinterpret_cast<ControlBlock*>(p_other.control_block)), object_offset(get_converted_offset(p_other)) {
        p_other.control_block = nullptr;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak(const Ref<T1, AllocType1>& p_ref)
        : control_block(reinterpret_cast<ControlBlock*>(p_ref.control_block)),
          object_offset(Ref<T, AllocType>::get_converted_offset(p_ref)) {
        ref();
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak& operator=(const RefWeak<T1, AllocType1>& p_other) {
        std::ptrdiff_t offset = get_converted_offset(p_other);
        p_other.ref();
        deref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        object_offset = offset;
        return *this;
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak& operator=(RefWeak<T1, AllocType1>&& p_other) {
        std::ptrdiff_t offset = get_converted_offset(p_other);
        deref();
        control_block = reinterpret_cast<ControlBlock*>(p_other.control_block);
        object_offset = offset;
        p_other.control_block = nullptr;
        return *this;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak& operator=(const Ref<T1, AllocType1>& p_ref) {
        std::ptrdiff_t offset = Ref<T, AllocType>::get_converted_offset(p_ref);
        ControlBlock* new_control_block = reinterpret_cast<ControlBlock*>(p_ref.control_block);
        increment(new_control_block);
        deref();
        control_block = new_control_block;
        object_offset = offset;
        return *this;
    }

    /**
     * @brief Acquire a strong reference to the object.
     *
     * @return The strong reference, or a null reference if the object has been destroyed or is being destroyed.
     */
    Ref<T, AllocType> lock() {
        return Ref<T, AllocType>::acquire_from_weak(control_block, object_offset);
    }

    /**
     * @brief Acquire a strong reference to the object.
     *
     * @return The strong reference, or a null reference if the object has been destroyed or is being destroyed.
     */
    Ref<const T, AllocType> lock() const {
        return Ref<const T, AllocType>::acquire_from_weak(control_block, object_offset);
    }

    /**
     * @brief Is the reference valid.
     *
     * @return True if the reference is valid, false otherwise.
     */
    bool is_valid() const {
        return control_block != nullptr && control_block->strong_ref_counter.load(std::memory_order_acquire) != 0;
    }

    template <typename T1, typename AllocType1>
    bool operator==(const RefWeak<T1, AllocType1>& p_other) const {
        return static_cast<const void*>(control_block) == static_cast<const void*>(p_other.control_block);
    }

    bool operator==(std::nullptr_t) const {
        return is_null();
    }

    /**
     * @brief Compare with MEM_NULL. An invalid reference equals MEM_NULL.
     *
     * @param p_mem_id Must be MEM_NULL.
     * @throws std::runtime_error If p_mem_id is not MEM_NULL.
     */
    bool operator==(MemID p_mem_id) const {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Cannot compare a weak reference with a memory ID that is not MEM_NULL.");
        }
        return is_null();
    }

    /**
     * @brief Is the reference NULL. Invalid reference would be recognized as
     * NULL.
     *
     * @return True if the reference is NULL, false otherwise.
     */
    bool is_null() const {
        return !(is_valid()) || control_block->allocator == nullptr || control_block->mem_id == MEM_NULL;
    }

private:
    mutable ControlBlock* control_block;
    std::ptrdiff_t object_offset = 0;

    template <typename T1, typename AllocType1>
    static std::ptrdiff_t get_converted_offset(const RefWeak<T1, AllocType1>& p_other) {
        // The object may be destroyed concurrently, so convert only through a strong reference. An expired reference
        // keeps offset 0, since it can never be locked again.
        Ref<T1, AllocType1> locked = Ref<T1, AllocType1>::acquire_from_weak(p_other.control_block, p_other.object_offset);
        return Ref<T, AllocType>::get_converted_offset(locked);
    }

    static void increment(ControlBlock* p_control_block) {
        if (p_control_block == nullptr) {
            return;
        }
        p_control_block->weak_ref_counter.fetch_add(1, std::memory_order_relaxed);
    }

    void ref() const {
        increment(control_block);
    }

    void deref() const {
        if (control_block == nullptr) {
            return;
        }
        uint64_t weak_ref_count = control_block->weak_ref_counter.fetch_sub(1, std::memory_order_acq_rel);
        if (weak_ref_count == 1) {
            // Every strong reference holds a hidden weak reference, so the last weak reference is also past the last
            // strong reference: destroy the control block.
            destroy_obj<ControlBlock>(*(control_block->allocator), control_block->control_block_mem_id);
        }
        control_block = nullptr;
    }
};

} // namespace WhiteBirdEngine

namespace std {
/**
 * @brief Hash function for weak reference. Hashes the control block, so the hash does not change when the object
 * expires.
 *
 * @tparam T The type of the reference.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType>
struct hash<::WhiteBirdEngine::RefWeak<T, AllocType>> { // NOLINT
    size_t operator()(const ::WhiteBirdEngine::RefWeak<T, AllocType>& p_ref) const {
        return std::hash<const void*>{}(static_cast<const void*>(p_ref.control_block));
    }
};
} // namespace std

#endif
