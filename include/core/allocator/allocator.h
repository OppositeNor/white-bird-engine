#ifndef __WBE_ALLOCATOR_H__
#define __WBE_ALLOCATOR_H__
#include <concepts>
#include <cstdint>
namespace WhiteBirdEngine {


using MemID = uintptr_t;
constexpr MemID MEM_NULL = 0;


template <typename T>
struct AllocatorTrait;

enum class AllocatorType {
    STACK_ALLOCATOR,
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
    // Is the memory of objects is guranteed to be continuous.
    { bool(T::IS_GURANTEED_CONTINUOUS) } -> std::same_as<bool>;
    // Is this allocator has limited size.
    { bool(T::IS_LIMITED_SIZE) } -> std::same_as<bool>;
    // Is the sizes of memories allocated from this allocator consistant.
    { bool(T::IS_ALLOC_FIXED_SIZE) } -> std::same_as<bool>;
    // Is operations on this allocator atomic.
    { bool(T::IS_ATOMIC) } -> std::same_as<bool>;
};
}

#endif
