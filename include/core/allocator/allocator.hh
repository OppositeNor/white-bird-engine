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
#ifndef __WBE_ALLOCATOR_HH__
#define __WBE_ALLOCATOR_HH__
#include <concepts>
#include <cstdint>
namespace WhiteBirdEngine {

using MemID = uintptr_t;
constexpr MemID MEM_NULL = 0;

template <typename T>
struct AllocatorTrait;

/**
 * @brief Types of allocators.
 */
enum class AllocatorType {
    // Stack allocator.
    STACK_ALLOCATOR,
    // Heap allocator.
    HEAP_ALLOCATOR
};

/**
 * @brief The traits of the allocator.
 *
 * @tparam T The type of the allocator.
 */
template <typename T>
concept AllocatorTraitConcept = requires {
    // The type of the allocator
    { AllocatorType(T::TYPE) } -> std::same_as<AllocatorType>;
    // Does this allocator allocates from a memory pool.
    { bool(T::IS_POOL) } -> std::same_as<bool>;
    // Is alignable
    { bool(T::IS_ALIGNABLE) } -> std::same_as<bool>;
    // Is the memory of objects is guranteed to be continuous.
    { bool(T::IS_GURANTEED_CONTINUOUS) } -> std::same_as<bool>;
    // Is this allocator has limited size.
    { bool(T::IS_LIMITED_SIZE) } -> std::same_as<bool>;
    // Is the sizes of memories allocated from this allocator consistant.
    { bool(T::IS_ALLOC_FIXED_SIZE) } -> std::same_as<bool>;
    // Is operations on this allocator atomic.
    { bool(T::IS_ATOMIC) } -> std::same_as<bool>;
    // Will the memory address be moved after allocated. If set to false,
    // the memory ID will be guranteed to be set to the memory address of
    // the allocated resource.
    { bool(T::WILL_ADDR_MOVE) } -> std::same_as<bool>;
};
}

#endif
