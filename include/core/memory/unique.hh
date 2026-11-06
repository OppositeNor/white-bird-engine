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
#ifndef WBE_FILE_UNIQUE_HH
#define WBE_FILE_UNIQUE_HH

#include "core/allocator/heap_allocator.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/subobject_offset.hh"
#include "utils/defs.hh"
#include <concepts>
#include <cstddef>
#include <functional>
#include <stdexcept>

namespace WhiteBirdEngine {

/**
 * @class Unique.
 * @brief Ensures an instance to only exists once, to make it memory safe.
 *
 * Movable, not copyable. A moved-from instance is null. An instance converted to a base type views that base
 * subobject.
 *
 * @tparam T The type of the instance.
 */
template <typename T, typename AllocType = HeapAllocator>
class Unique {
    template <typename T1, typename AllocType1>
    friend class Unique;
    template <typename T1, typename AllocType1, bool AllowDestruct>
    friend class RefRaw;

    friend struct ::std::hash<Unique<T, AllocType>>;

public:
    Unique() : mem_id(MEM_NULL), allocator(nullptr) {
    }
    ~Unique() {
        reset();
    }
    // Unique is not copyable.
    Unique(const Unique&) = delete;
    Unique(Unique&& p_other) noexcept
        : mem_id(p_other.mem_id), allocator(p_other.allocator), object_offset(p_other.object_offset) {
        p_other.mem_id = MEM_NULL;
    }
    Unique& operator=(const Unique&) = delete;
    Unique& operator=(Unique&& p_other) noexcept {
        if (this == &p_other) {
            return *this;
        }
        reset();
        mem_id = p_other.mem_id;
        allocator = p_other.allocator;
        object_offset = p_other.object_offset;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Unique(Unique<T1, AllocType1>&& p_other)
        : mem_id(p_other.mem_id), allocator(p_other.allocator),
          object_offset(get_converted_subobject_offset<T>(p_other.get(), p_other.object_offset)) {
        p_other.mem_id = MEM_NULL;
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    Unique& operator=(Unique<T1, AllocType1>&& p_other) {
        std::ptrdiff_t offset = get_converted_subobject_offset<T>(p_other.get(), p_other.object_offset);
        reset();
        mem_id = p_other.mem_id;
        allocator = p_other.allocator;
        object_offset = offset;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    Unique(AllocType* p_allocator, MemID p_mem_id) : mem_id(p_mem_id), allocator(p_allocator) {
    }

    Unique(MemID p_mem_id) : Unique() {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Allocator not specified.");
        }
    }

    /**
     * @brief Create a unique instance.
     *
     * @tparam Args The arguments of the constructor.
     * @param p_allocator The allocator that this instance is going to be
     * allocated.
     * @param p_args The arguments.
     * @return The unique instnace.
     */
    template <typename... Args>
    static Unique<T> make_unique(AllocType* p_allocator, Args&&... p_args) {
        return Unique(p_allocator, create_obj<T>(*p_allocator, std::forward<Args>(p_args)...));
    }

    /**
     * @brief Get the instance of the object.
     *
     * @return The instance of the object.
     */
    T* get() {
        if (is_null()) {
            return nullptr;
        }
        return get_object();
    }

    /**
     * @brief Get the instance of the object.
     *
     * @return The instance of the object.
     */
    const T* get() const {
        if (is_null()) {
            return nullptr;
        }
        return get_object();
    }

    /**
     * @brief Release the instance.
     */
    void reset() {
        if (is_null()) {
            return;
        }
        // Destroy through the viewed subobject, but free the whole allocation.
        get_object()->~T();
        allocator->deallocate(mem_id);
        mem_id = MEM_NULL;
        object_offset = 0;
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

    bool operator==(MemID p_mem_id) const {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Cannot compare a unique with a memory ID that is not MEM_NULL.");
        }
        return mem_id == MEM_NULL;
    }

    bool operator==(void* p_ptr) const {
        if (p_ptr != nullptr) {
            throw std::runtime_error("Cannot compare a unique with a pointer that is not nullptr.");
        }
        return mem_id == MEM_NULL;
    }

    bool operator==(std::nullptr_t) const {
        return mem_id == MEM_NULL;
    }

    /**
     * @brief Check if the unique reference is NULL.
     *
     * @return True if is NULL, false otherwise.
     */
    bool is_null() const {
        return allocator == nullptr || mem_id == MEM_NULL;
    }

private:
    T* get_object() const {
        WBE_DEBUG_ASSERT(allocator != nullptr);
        return apply_subobject_offset<T>(allocator->get(mem_id), object_offset);
    }

    MemID mem_id;
    AllocType* allocator;
    std::ptrdiff_t object_offset = 0;
};

/**
 * @brief Create a unique instance.
 *
 * @tparam T The type that is created.
 * @tparam Args The arguments of the constructor.
 * @param p_allocator The allocator that this instance is going to be allocated.
 * @param p_args The arguments.
 * @return The unique instnace.
 */
template <typename T, typename AllocType = HeapAllocator, typename... Args>
Unique<T> make_unique(AllocType* p_allocator, Args&&... p_args) {
    WBE_DEBUG_ASSERT(p_allocator != nullptr);
    MemID id = create_obj<T>(*p_allocator, std::forward<Args>(p_args)...);
    return Unique<T>(p_allocator, id);
}

} // namespace WhiteBirdEngine

namespace std {
/**
 * @brief Hash function for unique instance.
 *
 * @tparam T The type of the instance.
 * @param p_ref The reference to hash.
 * @return
 */
template <typename T, typename AllocType>
struct hash<::WhiteBirdEngine::Unique<T, AllocType>> { // NOLINT
    size_t operator()(const ::WhiteBirdEngine::Unique<T, AllocType>& p_unique) const {
        if (p_unique.is_null()) {
            return WhiteBirdEngine::MEM_NULL;
        }
        return std::hash<AllocType*>{}(p_unique.allocator) ^ std::hash<::WhiteBirdEngine::MemID>{}(p_unique.mem_id);
    }
};

} // namespace std

#endif
