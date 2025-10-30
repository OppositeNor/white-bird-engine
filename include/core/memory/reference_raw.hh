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
#include "core/allocator/heap_allocator.hh"
#include "core/allocator/stack_allocator.hh"
#include <concepts>
#include <cstddef>
#include <format>
#include <stdexcept>
namespace WhiteBirdEngine {

/**
 * @class RefRaw
 * @brief Raw reference. Act like raw pointers. Requires manual resource management.
 *
 * @tparam T The type of the resource.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType = HeapAllocator>
class RefRaw {
    template <typename T1, typename AllocType1>
    friend class RefRaw;

    friend struct ::std::hash<::WhiteBirdEngine::RefRaw<T, AllocType>>;
public:
    using ObjType = T;
    RefRaw() = default;
    ~RefRaw() {}
    RefRaw(const RefRaw& p_other)
        : mem_id(p_other.mem_id), num(p_other.num), allocator(p_other.allocator) {}
    RefRaw(RefRaw&& p_other)
        : mem_id(p_other.mem_id), num(p_other.num), allocator(p_other.allocator) {
        p_other.mem_id = MEM_NULL;
    }
    RefRaw& operator=(const RefRaw& p_other) {
        mem_id = p_other.mem_id;
        num = p_other.num;
        allocator = p_other.allocator;
        return *this;
    }
    RefRaw& operator=(RefRaw&& p_other) {
        mem_id = p_other.mem_id;
        num = p_other.num;
        allocator = p_other.allocator;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    /**
     * @brief Constructor.
     *
     * @param p_mem_id The memory ID.
     * @param p_allocator The allocator for the memory ID.
     */
    RefRaw(MemID p_mem_id, AllocType* p_allocator)
    : mem_id(p_mem_id), num(1), allocator(p_allocator) {}

    /**
     * @brief Constructor.
     *
     * @todo Test
     * @param p_mem_id The memory ID.
     */
    RefRaw(MemID p_mem_id)
    : mem_id(MEM_NULL), num(1), allocator(nullptr) {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Allocator not specified.");
        }
    }

    /**
     * @brief Constructor.
     *
     * @param p_mem_id The memory ID.
     * @param p_num The number of instance.
     * @param p_allocator The allocator for the memory ID.
     */
    RefRaw(MemID p_mem_id, size_t p_num, AllocType* p_allocator)
    : mem_id(p_mem_id), num(p_num), allocator(p_allocator) {
        if (p_num == 0) {
            throw std::runtime_error("Number of instance should be more than 0.");
        }
    }

    template <typename T1, typename AllocType1>
    requires std::convertible_to<T1, T> && (!std::same_as<T1, T>)
    RefRaw(const RefRaw<T1, AllocType1>& p_other)
    : mem_id(p_other.mem_id), num(p_other.num), allocator(p_other.allocator) {}
    template <typename T1, typename AllocType1>
    requires std::convertible_to<T1, T> && (!std::same_as<T1, T>)
    RefRaw(RefRaw<T1, AllocType1>&& p_other)
    : mem_id(p_other.mem_id), num(p_other.num), allocator(p_other.allocator) {
        p_other.mem_id = MEM_NULL;
    }
    template <typename T1, typename AllocType1>
    requires std::convertible_to<T1, T> && (!std::same_as<T1, T>)
    RefRaw& operator=(const RefRaw<T1, AllocType1>& p_other) {
        mem_id = p_other.mem_id;
        num = p_other.num;
        allocator = p_other.allocator;
        return *this;
    }
    template <typename T1, typename AllocType1>
    requires std::convertible_to<T1, T> && (!std::same_as<T1, T>)
    RefRaw& operator=(RefRaw<T1, AllocType1>&& p_other) {
        mem_id = p_other.mem_id;
        num = p_other.num;
        allocator = p_other.allocator;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    /**
     * @brief Create a reference.
     *
     * @tparam Args The type of the arguments.
     * @param p_allocator The allocator.
     * @param p_args The arguments passed into the constructor.
     * @return The ref raw instance.
     */
    template <typename... Args>
    static RefRaw<T, AllocType> new_ref(AllocType* p_allocator, Args&&... p_args) {
        if (p_allocator == nullptr) {
            throw std::runtime_error("Allocator cannot be nullptr.");
        }
        if constexpr (std::same_as<T, StackAllocator>) {
            MemID id = create_stack_obj<T>(*p_allocator, std::forward<Args>(p_args)...);
            return RefRaw<T, StackAllocator>(id, p_allocator);
        }
        else {
            if (p_allocator == nullptr) {
                throw std::runtime_error("Allocator cannot be nullptr.");
            }
            MemID id = create_obj<T>(*p_allocator, std::forward<Args>(p_args)...);
            return RefRaw<T, AllocType>(id, p_allocator);
        }
    }

    /**
     * @brief Delete a reference. Note that manual deleting from the allocator should
     * do the same job as calling this function. No other hidden operations are done.
     *
     * @param p_ref The reference to be deleted.
     */
    static void delete_ref(RefRaw<T, AllocType>&& p_ref) {
        if (p_ref.allocator == nullptr || p_ref.mem_id == MEM_NULL) {
            return;
        }
        if constexpr (std::same_as<T, StackAllocator>) {
            if (p_ref.num == 1) {
                pop_stack_obj<T>(*(p_ref.allocator), p_ref.mem_id);
            }
            else {
                pop_stack_obj_array<T>(*(p_ref.allocator), p_ref.mem_id);
            }
        }
        else {
            if (p_ref.num == 1) {
                destroy_obj<T>(*(p_ref.allocator), p_ref.mem_id);
            }
            else {
                destroy_array<T>(*(p_ref.allocator), p_ref.mem_id, p_ref.num);
            }
        }
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
    T* get(size_t p_index = 0) {
        if (allocator == nullptr) {
            return nullptr;
        }
        if (p_index >= num) {
            throw std::runtime_error(std::format("Failed to get instance at index: {}, index out of bounds.", p_index));
        }
        return &(static_cast<T*>(allocator->get(mem_id))[p_index]);
    }

    /**
     * @brief Get the resource pointer.
     *
     * @return The pointer pointing to the resource. nullptr if mem_id is MEM_NULL.
     */
    const T* get(size_t p_index = 0) const {
        if (allocator == nullptr) {
            return nullptr;
        }
        if (p_index >= num) {
            throw std::runtime_error(std::format("Failed to get instance at index: {}, index out of bounds.", p_index));
        }
        return &(static_cast<const T*>(allocator->get(mem_id))[p_index]);
    }

    template <typename T1, typename AllocType1>
    bool operator==(const RefRaw<T1, AllocType1>& p_other) const {
        return allocator == p_other.allocator && mem_id == p_other.mem_id;
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

    T& operator[](size_t p_index) {
        if (allocator == nullptr) {
            throw std::runtime_error("Cannot access elements of a RefRaw with null allocator.");
        }
        if (p_index >= num) {
            throw std::runtime_error(std::format("Failed to get instance at index: {}, index out of bounds.", p_index));
        }
        return static_cast<T*>(allocator->get(mem_id))[p_index];
    }

    const T& operator[](size_t p_index) const {
        if (allocator == nullptr) {
            throw std::runtime_error("Cannot access elements of a RefRaw with null allocator.");
        }
        if (p_index >= num) {
            throw std::runtime_error(std::format("Failed to get instance at index: {}, index out of bounds.", p_index));
        }
        return static_cast<const T*>(allocator->get(mem_id))[p_index];
    }

    /**
     * @brief Get the number of instances.
     *
     * @return The number of instances.
     */
    size_t get_num() const {
        return num;
    }

    /**
     * @brief Is the reference NULL.
     *
     * @return true if the reference is NULL, false otherwise.
     */
    bool is_null() const {
        return allocator == nullptr || mem_id == MEM_NULL;
    }

private:
    MemID mem_id = MEM_NULL;
    size_t num;
    AllocType* allocator = nullptr;
};

template <typename T, typename AllocType = HeapAllocator, typename... Args>
RefRaw<T, AllocType> new_ref(AllocType* p_allocator, Args&&... p_args) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Allocator cannot be nullptr.");
    }
    MemID id = create_obj<T>(*p_allocator, std::forward<Args>(p_args)...);
    return RefRaw<T, AllocType>(id, p_allocator);
}

template <typename T, typename AllocType = HeapAllocator>
void delete_ref(RefRaw<T, AllocType>&& p_ref) {
    RefRaw<T, AllocType>::delete_ref(std::move(p_ref));
}

template <typename T, typename... Args>
RefRaw<T, StackAllocator> new_ref_stack(StackAllocator* p_allocator, Args&&... p_args) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Allocator cannot be nullptr.");
    }
    MemID id = create_stack_obj<T>(*p_allocator, std::forward<Args>(p_args)...);
    return RefRaw<T, StackAllocator>(id, p_allocator);
}

template <typename T>
void delete_ref_stack(RefRaw<T, StackAllocator>&& p_ref) {
    RefRaw<T, StackAllocator>::delete_ref_stack(std::move(p_ref));
}
}

namespace std {
/**
 * @brief Hash function for raw reference.
 *
 * @tparam T The type of the reference.
 * @param p_ref The reference to hash.
 * @return 
 */
template <typename T, typename AllocType>
struct hash<::WhiteBirdEngine::RefRaw<T, AllocType>> {
    size_t operator()(const ::WhiteBirdEngine::RefRaw<T, AllocType>& p_ref) {
        if (p_ref.is_null()) {
            return WhiteBirdEngine::MEM_NULL;
        }
        return std::hash<AllocType*>{}(p_ref.allocator) ^ std::hash<::WhiteBirdEngine::MemID>{}(p_ref.control_block->mem_id);
    }

};
}

#endif
