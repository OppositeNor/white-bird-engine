#ifndef __WBE_ALLOCATOR_H__
#define __WBE_ALLOCATOR_H__

#include <memory>
#include <set>
#include <sstream>

namespace WhiteBirdEngine {

using MemID = uint64_t;

template <typename T>
struct AllocatorTrait;

/**
 * @brief The traits of the allocator.
 *
 * @tparam T The type of the allocator.
 */
template <typename T>
concept AllocatorTraitConcept = requires {
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

class Allocator {
public:
    Allocator() = default;
    virtual ~Allocator() {}

    /**
     * @brief Allocate memory of a specific size.
     * @note If IS_ALLOC_FIXED_SIZE is true for this allocator, allocating a size
     * that is not the same as the specified size for the allocator will cause an
     * error.
     *
     * @param p_size The size to allocate.
     * @return The memory ID of the allocated resource.
     */
    virtual MemID allocate(size_t p_size) = 0;

    /**
     * @brief Deallocate memroy.
     *
     * @param p_mem The memory id to be deallocated.
     */
    virtual void deallocate(MemID p_mem) = 0;

    /**
     * @brief Get the pointer pointing to the resource.
     *
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    virtual void* get(MemID p_id) = 0;

    /**
     * @brief Get the pointer pointing to the resource.
     *
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    virtual const void* get(MemID p_id) const = 0;

    /**
     * @brief Get the number of allocated resources.
     *
     * @return The number of allocated resources.
     */
    virtual uint32_t obj_count() const = 0;

    /**
     * @brief Get the allocated memories' ids
     */
    virtual const std::set<MemID>& get_allocated() const = 0;

    /**
     * @brief Get the pointer pointing to the resource.
     *
     * @tparam T The type of the resource.
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    template <typename T>
    T* get_obj(MemID p_id) {
        return static_cast<T*>(get(p_id));
    }

    virtual operator std::string() const {
        std::stringstream ss;
        ss << "Allocator{obj_count: " << obj_count() << "}";
        return ss.str();
    }
};

template <typename T>
concept FixedSizeAllocatorConcept = bool(AllocatorTrait<T>::IS_GURANTEED_CONTINUOUS);

template <typename T>
concept AtomicAllocatorConcept = bool(AllocatorTrait<T>::IS_ATOMIC);

/**
 * @brief Create an object.
 *
 * @tparam T The type of the object.
 * @tparam Args The types of the constructor arguments to construct the object.
 * @param p_allocator The allocator to allocate memory from.
 * @param p_args The arguments of the constructor.
 * @return The memory ID of the created object.
 */
template <typename T, typename... Args>
inline MemID create_obj(Allocator& p_allocator, Args&&... p_args) {
    MemID id = p_allocator.allocate(sizeof(T));
    new(p_allocator.get(id)) T(std::forward<Args>(p_args)...);
    return id;
}

/**
 * @brief Destroy an object.
 *
 * @tparam T The type of the object.
 * @param p_allocator The allocator that this object's memory is allocated from.
 * @param p_id The memory ID of the object.
 */
template <typename T, typename AllocType>
inline void destroy_obj(AllocType& p_allocator, MemID p_id) {
    static_cast<T*>(p_allocator.get(p_id))->~T();
    p_allocator.deallocate(p_id);
}

}

#endif
