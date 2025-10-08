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
#ifndef __WBE_UNIQUE_HH__
#define __WBE_UNIQUE_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include <concepts>
#include <cstddef>
#include <stdexcept>

namespace WhiteBirdEngine {

/**
 * @class Unique.
 * @brief Ensures an instance to only exists once, to make it memory safe.
 *
 * @tparam T The type of the instance.
 */
template <typename T, typename AllocType = HeapAllocatorAligned>
class Unique {
    template <typename T1, typename AllocType1>
    friend class Unique;

public:
    Unique()
        : mem_id(MEM_NULL), allocator(nullptr) {}
    ~Unique() {
        reset();
    }
    // Unique is not copyable.
    Unique(const Unique&) = delete;
    Unique(Unique&& p_other)
        : mem_id(p_other.mem_id), allocator(p_other.allocator) {
        p_other.mem_id = MEM_NULL;
    }
    Unique& operator=(const Unique&) = delete;
    Unique& operator=(Unique&& p_other) {
        reset();
        mem_id = p_other.mem_id;
        allocator = p_other.allocator;
        p_other.mem_id = MEM_NULL;
        return *this;
    }

    template <typename T1> requires std::convertible_to<T1*, T*>
    Unique(Unique<T1>&& p_other)
        : mem_id(p_other.mem_id), allocator(p_other.allocator) {
        p_other.mem_id = MEM_NULL;
    }
    template <typename T1> requires std::convertible_to<T1*, T*>
    Unique& operator=(Unique<T1>&& p_other) {
        reset();
        mem_id = p_other.mem_id;
        allocator = p_other.allocator;
        p_other.mem_id = MEM_NULL;
        return *this;
    }
    
    Unique(AllocType* p_allocator, MemID p_mem_id)
        : mem_id(p_mem_id), allocator(p_allocator) {}

    Unique(MemID p_mem_id)
        : Unique() {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Allocator not specified.");
        }
    }

    /**
     * @brief Create a unique instance.
     *
     * @tparam Args The arguments of the constructor.
     * @param p_allocator The allocator that this instance is going to be allocated.
     * @param p_args The arguments.
     * @return The unique instnace.
     */
    template <typename... Args>
    static Unique<T> make_unique(AllocType* p_allocator, Args&&... p_args) {
        return Unique(p_allocator, create_obj_align<T>(*p_allocator, std::forward<Args>(p_args)...));
    }

    /**
     * @brief Get the instance of the object.
     *
     * @return The instance of the object.
     */
    T* get() {
        if (allocator == nullptr) {
            return nullptr;
        }
        return allocator->template get_obj<T>(mem_id);
    }

    /**
     * @brief Get the instance of the object.
     *
     * @return The instance of the object.
     */
    const T* get() const {
        if (allocator == nullptr) {
            return nullptr;
        }
        return allocator->template get_obj<T>(mem_id);
    }

    /**
     * @brief Release the instance.
     */
    void reset() {
        if (allocator != nullptr && mem_id != MEM_NULL) {
            destroy_obj<T>(*allocator, mem_id);
            mem_id = MEM_NULL;
        }
    }

    T* operator->() {
        return static_cast<T*>(allocator->get(mem_id));
    }

    const T* operator->() const {
        return static_cast<T*>(allocator->get(mem_id));
    }

    T& operator*() {
        return *static_cast<T*>(allocator->get(mem_id));
    }

    const T& operator*() const {
        return *static_cast<T*>(allocator->get(mem_id));
    }

    bool operator==(MemID p_mem_id) {
        if (p_mem_id != MEM_NULL) {
            throw std::runtime_error("Cannot compare a unique with a memory ID that is not MEM_NULL.");
        }
        return mem_id == MEM_NULL;
    }

    bool operator==(void* p_ptr) {
        if (p_ptr != nullptr) {
            throw std::runtime_error("Cannot compare a unique with a pointer that is not nullptr.");
        }
        return mem_id == MEM_NULL;
    }

    bool operator==(std::nullptr_t) {
        return mem_id == MEM_NULL;
    }

private:
    MemID mem_id;
    AllocType* allocator;
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
template <typename T, typename AllocType = HeapAllocatorAligned,  typename... Args>
Unique<T> make_unique(AllocType* p_allocator, Args&&... p_args) {
    WBE_DEBUG_ASSERT(p_allocator != nullptr);
    MemID id = create_obj_align<T>(*p_allocator, std::forward<Args>(p_args)...);
    return Unique<T>(p_allocator, id);
}

}

#endif
