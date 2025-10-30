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
#ifndef __WBE_REFERENCE_WEAK_HH__
#define __WBE_REFERENCE_WEAK_HH__

#include "core/allocator/heap_allocator.hh"
#include "reference_strong.hh"
#include <atomic>
#include <concepts>

namespace WhiteBirdEngine {

template <typename T, typename AllocType = HeapAllocator>
class RefWeak {
public:
    using ObjType = T;

    RefWeak()
        : control_block(nullptr) {}
    ~RefWeak() {
        deref();
    }
    RefWeak(const RefWeak<T, AllocType>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        p_other.ref();
        control_block = p_other.control_block;
    }
    RefWeak(RefWeak<T, AllocType>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        control_block = p_other.control_block;
        p_other.control_block = nullptr;
    }
    RefWeak& operator=(const RefWeak<T, AllocType>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        if (*this == p_other) {
            return *this;
        }
        deref();
        p_other.ref();
        control_block = p_other.control_block;
        return *this;
    }
    RefWeak& operator=(RefWeak<T, AllocType>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        if (*this == p_other) {
            return *this;
        }
        deref();
        control_block = p_other.control_block;
        p_other.control_block = nullptr;
        return *this;
    }
    RefWeak(const Ref<T, AllocType>& p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        control_block = p_ref.control_block;
        ref();
    }
    RefWeak& operator=(const Ref<T, AllocType>& p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        deref();
        control_block = p_ref.control_block;
        ref();
        return *this;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak(const RefWeak<T1, AllocType1>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        p_other.ref();
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak(RefWeak<T1, AllocType1>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
        p_other.control_block = nullptr;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak(const Ref<T1, AllocType1>& p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        control_block = reinterpret_cast<decltype(control_block)>(p_ref.control_block);
        ref();
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak& operator=(const RefWeak<T1, AllocType1>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        if (*this == p_other) {
            return *this;
        }
        deref();
        p_other.ref();
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
        return *this;
    }
    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak& operator=(RefWeak<T1, AllocType1>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        if (*this == p_other) {
            return *this;
        }
        deref();
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
        p_other.control_block = nullptr;
        return *this;
    }

    template <typename T1, typename AllocType1>
        requires std::convertible_to<T1*, T*> && std::convertible_to<AllocType1*, AllocType*>
    RefWeak& operator=(const Ref<T1, AllocType1>& p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        deref();
        control_block = reinterpret_cast<decltype(control_block)>(p_ref.control_block);
        ref();
        return *this;
    }

    Ref<T> lock() {
        if (!is_valid()) {
            return Ref<T>(nullptr);
        }
        return Ref<T>(control_block);
    }

    Ref<const T> lock() const {
        if (!is_valid()) {
            return Ref<const T>(nullptr);
        }
        return Ref<const T>(control_block);
    }

    /**
     * @brief Is the reference valid.
     *
     * @return True if the reference is valid, false otherwise.
     */
    bool is_valid() const {
        return control_block != nullptr && control_block->strong_ref_counter.load(std::memory_order_acquire) != 0;
    }

    template <typename T1>
    bool operator==(const RefWeak<T1>& p_other) const {
        return control_block == reinterpret_cast<decltype(control_block)>(p_other.control_block);
    }

    /**
     * @brief Is the reference NULL. Invalid reference would be recognized as NULL.
     *
     * @return True if the reference is NULL, false otherwise.
     */
    bool is_null() const {
        if (!is_valid()) {
            return false;
        }
        return control_block->allocator == nullptr || control_block->mem_id == MEM_NULL;
    }

private:
    mutable Ref<T>::ControlBlock* control_block;

    void ref() const {
        if (control_block == nullptr) {
            return;
        }
        control_block->weak_ref_counter.fetch_add(1, std::memory_order_acq_rel);
    }

    void deref() const {
        if (control_block == nullptr) {
            return;
        }
        uint32_t weak_ref_count = control_block->weak_ref_counter.fetch_sub(1, std::memory_order_acq_rel) - 1;
        uint32_t total_count = control_block->strong_ref_counter.load(std::memory_order_acquire) + weak_ref_count;
        if (weak_ref_count == 0 && total_count == 0) {
            destroy_obj<typename Ref<T>::ControlBlock>(*(control_block->allocator), control_block->control_block_mem_id);
        }
        control_block = nullptr;
    }
};

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
struct hash<::WhiteBirdEngine::RefWeak<T, AllocType>> {
    size_t operator()(const ::WhiteBirdEngine::RefWeak<T, AllocType>& p_ref) {
        if (p_ref.is_null()) {
            return WhiteBirdEngine::MEM_NULL;
        }
        return std::hash<AllocType*>{}(p_ref.control_block->allocator) ^ std::hash<::WhiteBirdEngine::MemID>{}(p_ref.control_block->mem_id);
    }

};
}

#endif
