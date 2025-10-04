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
#ifndef __WBE_REFERENCE_RAW_HH__
#define __WBE_REFERENCE_RAW_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include <concepts>
#include <stdexcept>
namespace WhiteBirdEngine {

/**
 * @class RefRaw
 * @brief Raw reference. Act like raw pointers. Requires manual resource management.
 *
 * @tparam T The type of the resource.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType = HeapAllocatorAligned>
class RefRaw {
    template <typename T1, typename AllocType1>
    friend class RefRaw;
    template <typename T1, typename AllocType1>
    friend void delete_ref(RefRaw<T, AllocType>&&);
public:
    using ObjType = T;
    RefRaw() = default;
    ~RefRaw() {}
    RefRaw(const RefRaw& p_other)
        : mem_id(p_other.mem_id), allocator(p_other.allocator) {}
    RefRaw(RefRaw&& p_other)
        : mem_id(p_other.mem_id), allocator(p_other.allocator) {
        p_other.mem_id = MEM_NULL;
    }
    RefRaw& operator=(const RefRaw& p_other) {
        mem_id = p_other.mem_id;
        allocator = p_other.allocator;
        return *this;
    }
    RefRaw& operator=(RefRaw&& p_other) {
        mem_id = p_other.mem_id;
        allocator = p_other.allocator;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    RefRaw(MemID p_mem_id, AllocType* p_allocator)
        : mem_id(p_mem_id), allocator(p_allocator) {}

    template <typename T1, typename AllocType1> requires std::convertible_to<T1, T> && std::convertible_to<AllocType1, AllocType>
    RefRaw(const RefRaw<T1, AllocType1>& p_other)
        : mem_id(p_other.mem_id), allocator(p_other.allocator) {}
    template <typename T1, typename AllocType1> requires std::convertible_to<T1, T> && std::convertible_to<AllocType1, AllocType>
    RefRaw(RefRaw<T1, AllocType1>&& p_other)
        : mem_id(p_other.mem_id), allocator(p_other.allocator) {
        p_other.mem_id = MEM_NULL;
    }
    template <typename T1, typename AllocType1> requires std::convertible_to<T1, T> && std::convertible_to<AllocType1, AllocType>
    RefRaw& operator=(const RefRaw<T1, AllocType1>& p_other) {
        mem_id = p_other.mem_id;
        allocator = p_other.allocator;
        return *this;
    }
    template <typename T1, typename AllocType1> requires std::convertible_to<T1, T> && std::convertible_to<AllocType1, AllocType>
    RefRaw& operator=(RefRaw<T1, AllocType1>&& p_other) {
        mem_id = p_other.mem_id;
        allocator = p_other.allocator;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    template <typename... Args>
    static RefRaw<T, AllocType> new_ref(AllocType* p_allocator, Args&&... p_args) {
        if (p_allocator == nullptr) {
            throw std::runtime_error("Allocator cannot be nullptr.");
        }
        MemID id = create_obj_align<T>(*p_allocator, std::forward<Args>(p_args)...);
        return RefRaw<T, AllocType>(id, p_allocator);

    }

    static void delete_ref(RefRaw<T, AllocType>&& p_ref) {
        destroy_obj<T>(p_ref.allocator, p_ref.mem_id);
        p_ref.mem_id = MEM_NULL;
    }

    T* operator->() {
        WBE_DEBUG_ASSERT(allocator != nullptr);
        return static_cast<T*>(allocator->get(mem_id));
    }

    const T* operator->() const {
        WBE_DEBUG_ASSERT(allocator != nullptr);
        return static_cast<T*>(allocator->get(mem_id));
    }

    T& operator*() {
        WBE_DEBUG_ASSERT(allocator != nullptr);
        return *static_cast<T*>(allocator->get(mem_id));
    }

    const T& operator*() const {
        WBE_DEBUG_ASSERT(allocator != nullptr);
        return *static_cast<T*>(allocator->get(mem_id));
    }

    /**
     * @brief Get the resource pointer.
     *
     * @return The pointer pointing to the resource. nullptr if mem_id is MEM_NULL.
     */
    T* get() {
        if (allocator == nullptr) {
            return nullptr;
        }
        WBE_DEBUG_ASSERT(allocator != nullptr);
        return static_cast<T*>(allocator->get(allocator->mem_id));
    }

    /**
     * @brief Get the resource pointer.
     *
     * @return The pointer pointing to the resource. nullptr if mem_id is MEM_NULL.
     */
    const T* get() const {
        if (allocator == nullptr) {
            return nullptr;
        }
        WBE_DEBUG_ASSERT(allocator != nullptr);
        return static_cast<T*>(allocator->get(allocator->mem_id));
    }

    template <typename T1>
    bool operator==(const RefRaw<T1>& p_other) const {
        return allocator == p_other.allocator && mem_id == p_other.mem_id;
    }

    bool operator==(std::nullptr_t) const {
        return allocator == nullptr || mem_id == MEM_NULL;
    }

    bool operator==(void* p_ptr) const {
        if (p_ptr != nullptr) {
            throw std::runtime_error("Cannot compare a unique with a pointer that is not nullptr.");
        }
        return allocator == nullptr || mem_id == MEM_NULL;
    }

    bool operator==(MemID p_mem_id) const {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Cannot compare a unique with a memory ID that is not MEM_NULL.");
        }
        return allocator == nullptr || mem_id == MEM_NULL;
    }

    template <typename T1>
    bool operator!=(T1 p_obj) const {
        return !(*this == p_obj);
    }

private:
    MemID mem_id = MEM_NULL;
    AllocType* allocator = nullptr;
};

template <typename T, typename AllocType = HeapAllocatorAligned, typename... Args>
RefRaw<T, AllocType> new_ref(AllocType* p_allocator, Args&&... p_args) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Allocator cannot be nullptr.");
    }
    MemID id = create_obj_align<T>(*p_allocator, std::forward<Args>(p_args)...);
    return RefRaw<T, AllocType>(id, p_allocator);

}

template <typename T, typename AllocType = HeapAllocatorAligned>
void delete_ref(RefRaw<T, AllocType>&& p_ref) {
    destroy_obj<T>(p_ref.allocator, p_ref.mem_id);
    p_ref.mem_id = MEM_NULL;
}

}

#endif
