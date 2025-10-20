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
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include <stdexcept>
#include <string>
namespace WhiteBirdEngine {

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

using HeapAllocatorDefault = HeapAllocatorAlignedPool;

}

#endif
