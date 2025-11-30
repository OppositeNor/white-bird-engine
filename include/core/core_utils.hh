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
#ifndef __CORE_UTILS_HH__
#define __CORE_UTILS_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned_pool_impl_list.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include <stdexcept>
#include <string>
namespace WhiteBirdEngine {

/**
 * @class ThisRef
 * @brief Interface for objects that could be able to get a reference of themselves (this_ref).
 * If this object is not created as a reference, this_ref will be MEM_NULL.
 *
 * @tparam T The type of the class.
 */
template <typename T>
class ThisRef {
public:

    ThisRef() {}
    virtual ~ThisRef() {}

    /**
     * @brief Set the reference referencing this instance.
     *
     * @param p_ref_of_this The reference referencing this instance.
     */
    void set_ref_of_this(Ref<T> p_ref_of_this) {
        WBE_DEBUG_ASSERT(this_ref.is_null());
        this_ref = p_ref_of_this;
    }

protected:
    RefWeak<T> this_ref;
};

template <typename T>
T& required(const std::string& p_name, Ref<T> p_ref) {
    if (p_ref == MEM_NULL) {
        throw std::runtime_error("Reference: \"" + p_name + "\" is required.");
    }
    return *p_ref;
}

template <typename T>
Ref<T> requires_valid(const std::string& p_name, Ref<T> p_ref) {
    if (p_ref == MEM_NULL) {
        throw std::runtime_error("Reference: \"" + p_name + "\" is required to be valid.");
    }
    return p_ref;
}

template <typename T>
RefWeak<T> requires_valid(const std::string& p_name, RefWeak<T> p_ref) {
    if (!p_ref.is_valid() || p_ref == MEM_NULL) {
        throw std::runtime_error("Weak reference: \"" + p_name + "\" is required to be valid.");
    }
    return p_ref;
}

using HeapAllocatorDefault = HeapAllocatorAlignedPoolImplicitList;

}

#endif
