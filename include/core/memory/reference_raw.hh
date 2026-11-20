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
#ifndef WBE_FILE_REFERENCE_RAW_HH
#define WBE_FILE_REFERENCE_RAW_HH

#include "core/alloc/heap_alloc.hh"
#include "core/alloc/i_alloc.hh"
#include "core/alloc/stack_alloc.hh"
#include "core/memory/subobject_offset.hh"
#include "core/memory/unique.hh"
#include "utils/defs.hh"
#include <concepts>
#include <cstddef>
#include <functional>
#include <stdexcept>
namespace WhiteBirdEngine {

/**
 * @class RefRaw
 * @brief Raw reference. Act like raw pointers. Requires manual resource
 * management.
 *
 * Copyable and movable. A moved-from reference is null. A reference converted to a base type views that base
 * subobject, while delete_ref() still frees the whole allocation.
 *
 * @tparam T The type of the resource.
 * @tparam AllocType The type of the allocator.
 */
template <typename T, typename AllocType = HeapAlloc, bool AllowDestruct = false>
class RefRaw {
    template <typename T1, typename AllocType1, bool AllowDestruct1>
    friend class RefRaw;

    friend struct ::std::hash<::WhiteBirdEngine::RefRaw<T, AllocType, AllowDestruct>>;

public:
    using ObjType = T;
    RefRaw() = default;
    ~RefRaw() = default;
    RefRaw(const RefRaw& p_other)
        : mem_id(p_other.mem_id), alloc(p_other.alloc), object_offset(p_other.object_offset) {
    }
    RefRaw(RefRaw&& p_other) noexcept
        : mem_id(p_other.mem_id), alloc(p_other.alloc), object_offset(p_other.object_offset) {
        p_other.mem_id = MEM_NULL;
    }
    RefRaw& operator=(const RefRaw& p_other) {
        if (&p_other == this) {
            return *this;
        }
        mem_id = p_other.mem_id;
        alloc = p_other.alloc;
        object_offset = p_other.object_offset;
        return *this;
    }
    RefRaw& operator=(RefRaw&& p_other) noexcept {
        if (&p_other == this) {
            return *this;
        }
        mem_id = p_other.mem_id;
        alloc = p_other.alloc;
        object_offset = p_other.object_offset;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    /**
     * @brief Constructor.
     *
     * @param p_mem_id The memory ID.
     * @param p_alloc The allocator for the memory ID.
     */
    RefRaw(MemID p_mem_id, AllocType* p_alloc) : mem_id(p_mem_id), alloc(p_alloc) {
    }

    /**
     * @brief Constructor.
     *
     * @todo Test
     * @param p_mem_id The memory ID.
     */
    RefRaw(MemID p_mem_id) : alloc(nullptr) {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Allocator not specified.");
        }
    }

    template <typename T1, typename AllocType1, bool AllowDestruct1>
        requires std::convertible_to<T1*, T*> &&
                     (!std::same_as<T1, T> || !std::same_as<AllocType1, AllocType> || AllowDestruct1 != AllowDestruct)
    RefRaw(const RefRaw<T1, AllocType1, AllowDestruct1>& p_other)
        : mem_id(p_other.mem_id), alloc(p_other.alloc), object_offset(p_other.template get_converted_offset<T>()) {
    }
    template <typename T1, typename AllocType1, bool AllowDestruct1>
        requires std::convertible_to<T1*, T*> &&
                     (!std::same_as<T1, T> || !std::same_as<AllocType1, AllocType> || AllowDestruct1 != AllowDestruct)
    RefRaw(RefRaw<T1, AllocType1, AllowDestruct1>&& p_other)
        : mem_id(p_other.mem_id), alloc(p_other.alloc), object_offset(p_other.template get_converted_offset<T>()) {
        p_other.mem_id = MEM_NULL;
    }
    template <typename T1, typename AllocType1, bool AllowDestruct1>
        requires std::convertible_to<T1*, T*> &&
                 (!std::same_as<T1, T> || !std::same_as<AllocType1, AllocType> || AllowDestruct1 != AllowDestruct)
    RefRaw& operator=(const RefRaw<T1, AllocType1, AllowDestruct1>& p_other) {
        object_offset = p_other.template get_converted_offset<T>();
        mem_id = p_other.mem_id;
        alloc = p_other.alloc;
        return *this;
    }
    RefRaw& operator=(std::nullptr_t) {
        mem_id = MEM_NULL;
        object_offset = 0;
        return *this;
    }
    template <typename T1, typename AllocType1, bool AllowDestruct1>
        requires std::convertible_to<T1*, T*> &&
                 (!std::same_as<T1, T> || !std::same_as<AllocType1, AllocType> || AllowDestruct1 != AllowDestruct)
    RefRaw& operator=(RefRaw<T1, AllocType1, AllowDestruct1>&& p_other) {
        object_offset = p_other.template get_converted_offset<T>();
        mem_id = p_other.mem_id;
        alloc = p_other.alloc;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*> && (!AllowDestruct)
    RefRaw(Unique<T1, AllocType1>& p_reference)
        : mem_id(p_reference.mem_id), alloc(p_reference.alloc),
          object_offset(get_converted_subobject_offset<T>(p_reference.get(), p_reference.object_offset)) {
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*> && (!AllowDestruct)
    RefRaw& operator=(Unique<T1, AllocType1>& p_reference) {
        object_offset = get_converted_subobject_offset<T>(p_reference.get(), p_reference.object_offset);
        mem_id = p_reference.mem_id;
        alloc = p_reference.alloc;
        return *this;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && (!std::same_as<T1, T>)
    operator RefRaw<T1, AllocType1, AllowDestruct>() {
        RefRaw<T1, AllocType1, AllowDestruct> converted(mem_id, alloc);
        converted.object_offset = get_converted_offset<T1>();
        return converted;
    }

    /**
     * @brief Reinterpret cast this reference to T1.
     *
     * @tparam T1 The type to cast to.
     * @return The casted reference.
     */
    template <typename T1, typename AllocType1 = AllocType>
    RefRaw<T1, AllocType1, AllowDestruct> force_cast() {
        RefRaw<T1, AllocType1, AllowDestruct> result(mem_id, alloc);
        // Reinterpreting keeps the viewed address.
        result.object_offset = object_offset;
        return result;
    }

    /**
     * @brief Create a reference.
     *
     * @tparam Args The type of the arguments.
     * @param p_alloc The allocator.
     * @param p_args The arguments passed into the constructor.
     * @return The ref raw instance.
     */
    template <typename... Args>
    static RefRaw<T, AllocType, AllowDestruct> new_ref(AllocType* p_alloc, Args&&... p_args) {
        if (p_alloc == nullptr) {
            throw std::runtime_error("Allocator cannot be nullptr.");
        }
        if constexpr (std::same_as<AllocType, StackAlloc>) {
            MemID id = create_stack_obj<T>(*p_alloc, std::forward<Args>(p_args)...);
            return RefRaw<T, StackAlloc, AllowDestruct>(id, p_alloc);
        } else {
            MemID id = create_obj<T>(*p_alloc, std::forward<Args>(p_args)...);
            return RefRaw<T, AllocType, AllowDestruct>(id, p_alloc);
        }
    }

    /**
     * @brief Delete a reference. Note that manual deleting from the allocator
     * should do the same job as calling this function. No other hidden
     * operations are done. The object is destroyed through the viewed type, and the whole allocation is freed.
     *
     * @param p_ref The reference to be deleted. For a StackAlloc, it must be the top of the stack.
     * @throws std::runtime_error If a stack reference is not the top of the stack.
     */
    static void delete_ref(RefRaw<T, AllocType, AllowDestruct>&& p_ref) {
        if (p_ref.is_null()) {
            return;
        }
        if constexpr (AllowDestruct) {
            void* allocation = p_ref.alloc->get(p_ref.mem_id);
            p_ref.get_object()->~T();
            if constexpr (std::same_as<AllocType, StackAlloc>) {
                if (p_ref.alloc->pop_stack(sizeof(T)) != allocation) {
                    throw std::runtime_error("Stack references must be deleted in reverse order of creation.");
                }
            } else {
                p_ref.alloc->deallocate(p_ref.mem_id);
            }
        }
        p_ref.mem_id = MEM_NULL;
        p_ref.object_offset = 0;
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

    template <typename T1, typename AllocType1, bool AllowDestruct1>
    bool operator==(const RefRaw<T1, AllocType1, AllowDestruct1>& p_other) const {
        return alloc == p_other.alloc && mem_id == p_other.mem_id;
    }

    template <typename T1, typename AllocType1, bool AllowDestruct1>
    bool operator<(const RefRaw<T1, AllocType1, AllowDestruct1>& p_other) const {
        const void* alloc_address = static_cast<const void*>(alloc);
        const void* other_alloc_address = static_cast<const void*>(p_other.alloc);
        if (alloc_address < other_alloc_address) {
            return true;
        }
        if (other_alloc_address < alloc_address) {
            return false;
        }
        return mem_id < p_other.mem_id;
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
     * @brief Is the reference NULL.
     *
     * @return true if the reference is NULL, false otherwise.
     */
    bool is_null() const {
        return alloc == nullptr || mem_id == MEM_NULL;
    }

private:
    T* get_object() const {
        WBE_DEBUG_ASSERT(alloc != nullptr);
        return apply_subobject_offset<T>(alloc->get(mem_id), object_offset);
    }

    template <typename T1>
    std::ptrdiff_t get_converted_offset() const {
        if (is_null()) {
            return 0;
        }
        return get_converted_subobject_offset<T1>(get_object(), object_offset);
    }

    MemID mem_id = MEM_NULL;
    AllocType* alloc = nullptr;
    std::ptrdiff_t object_offset = 0;
};

template <typename T, typename AllocType = HeapAlloc, bool AllowDestruct = true, typename... Args>
RefRaw<T, AllocType, AllowDestruct> new_ref(AllocType* p_alloc, Args&&... p_args) {
    if (p_alloc == nullptr) {
        throw std::runtime_error("Allocator cannot be nullptr.");
    }
    MemID id = create_obj<T>(*p_alloc, std::forward<Args>(p_args)...);
    return RefRaw<T, AllocType, AllowDestruct>(id, p_alloc);
}

template <typename T, typename AllocType = HeapAlloc, bool AllowDestruct = true>
void delete_ref(RefRaw<T, AllocType, AllowDestruct>&& p_ref) {
    RefRaw<T, AllocType, AllowDestruct>::delete_ref(std::move(p_ref));
}

template <typename T, bool AllowDestruct = true, typename... Args>
RefRaw<T, StackAlloc, AllowDestruct> new_ref_stack(StackAlloc* p_alloc, Args&&... p_args) {
    if (p_alloc == nullptr) {
        throw std::runtime_error("Allocator cannot be nullptr.");
    }
    MemID id = create_stack_obj<T>(*p_alloc, std::forward<Args>(p_args)...);
    return RefRaw<T, StackAlloc, AllowDestruct>(id, p_alloc);
}

template <typename T, bool AllowDestruct = true>
void delete_ref_stack(RefRaw<T, StackAlloc, AllowDestruct>&& p_ref) {
    RefRaw<T, StackAlloc, AllowDestruct>::delete_ref(std::move(p_ref));
}
} // namespace WhiteBirdEngine

namespace std {
/**
 * @brief Hash function for raw reference.
 *
 * @tparam T The type of the reference.
 * @param p_ref The reference to hash.
 * @return
 */
template <typename T, typename AllocType, bool AllowDestruct>
struct hash<::WhiteBirdEngine::RefRaw<T, AllocType, AllowDestruct>> { // NOLINT
    size_t operator()(const ::WhiteBirdEngine::RefRaw<T, AllocType, AllowDestruct>& p_ref) const {
        if (p_ref.is_null()) {
            return WhiteBirdEngine::MEM_NULL;
        }
        return std::hash<AllocType*>{}(p_ref.alloc) ^ std::hash<::WhiteBirdEngine::MemID>{}(p_ref.mem_id);
    }
};
} // namespace std

#endif
