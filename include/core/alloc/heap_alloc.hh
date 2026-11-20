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
#ifndef WBE_FILE_HEAP_ALLOC_HH
#define WBE_FILE_HEAP_ALLOC_HH

#include "i_alloc.hh"
#include <cstddef>
#include <sstream>
#include <string>

namespace WhiteBirdEngine {

template <>
struct AllocTrait<class HeapAlloc> {
    static constexpr AllocType TYPE = AllocType::HEAP_ALLOC;
};

/**
 * @class HeapAlloc
 * @brief Heap allocator.
 *
 */
class HeapAlloc : public IAlloc {
public:
    HeapAlloc() = default;
    virtual ~HeapAlloc() override = default;
    HeapAlloc(const HeapAlloc&) = delete;
    HeapAlloc(HeapAlloc&&) = delete;
    HeapAlloc& operator=(const HeapAlloc&) = delete;
    HeapAlloc& operator=(HeapAlloc&&) = delete;

    /**
     * @brief Allocate memory of a specific size.
     * @note If IS_ALLOC_FIXED_SIZE is true for this allocator, allocating a
     * size that is not the same as the specified size for the allocator will
     * cause an error.
     *
     * @param p_size The size to allocate.
     * @return The memory ID of the allocated resource.
     */
    virtual MemID allocate(size_t p_size) = 0;

    /**
     * @brief Try to allocate memory of a specific size.
     *
     * @param p_size The size to allocate.
     * @return The memory ID of the allocated resource, or MEM_NULL if no space is available.
     */
    virtual MemID try_allocate(size_t p_size) {
        return allocate(p_size);
    }

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
    virtual void* get(MemID p_id) const override = 0;

    /**
     * @brief Is the allocator empty.
     *
     * @return true if allocator is empty, false otherwise.
     */
    virtual bool is_empty() const = 0;

    /**
     * @brief Clear the allocator.
     */
    virtual void clear() = 0;

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

    /**
     * @brief Get the pointer pointing to the resource.
     *
     * @tparam T The type of the resource.
     * @param p_id The resource id to get the pointer from.
     * @return The pointer of the resource.
     */
    template <typename T>
    const T* get_obj(MemID p_id) const {
        return static_cast<const T*>(get(p_id));
    }

    virtual operator std::string() const {
        std::stringstream ss;
        ss << R"({"type":"HeapAlloc"})";
        return ss.str();
    }
};

/**
 * @brief Create an object.
 *
 * @tparam T The type of the object.
 * @tparam Args The types of the constructor arguments to construct the object.
 * @param p_alloc The allocator to allocate memory from.
 * @param p_args The arguments of the constructor.
 * @return The memory ID of the created object.
 */
template <typename T, typename... Args>
inline MemID create_obj(HeapAlloc& p_alloc, Args&&... p_args) {
    MemID id = p_alloc.allocate(sizeof(T));
    new (p_alloc.get(id)) T(std::forward<Args>(p_args)...);
    return id;
}

/**
 * @brief Destroy an object.
 *
 * @tparam T The type of the object.
 * @param p_alloc The allocator that this object's memory is allocated from.
 * @param p_id The memory ID of the object.
 */
template <typename T, typename AllocType>
inline void destroy_obj(AllocType& p_alloc, MemID p_id) {
    if (p_id == MEM_NULL) {
        return;
    }
    static_cast<T*>(p_alloc.get(p_id))->~T();
    p_alloc.deallocate(p_id);
}

/**
 * @brief Create an array.
 *
 * @tparam T The type of the array element.
 * @tparam AllocType The type of the allocator.
 * @tparam Args The arguments types that will be passed to each element of the
 * constructor.
 * @param p_alloc The allocator to allocate the array.
 * @param p_num The size of the array.
 * @param p_args The arguments that will be passed to each element of the
 * constructor.
 * @return The array created.
 */
template <typename T, typename AllocType, typename... Args>
inline MemID create_array(AllocType& p_alloc, size_t p_num, Args&&... p_args) {
    T* begin = reinterpret_cast<T*>(p_alloc.allocate(sizeof(T) * p_num));
    for (size_t i = 0; i < p_num; ++i) {
        new (begin + i) T(std::forward<Args>(p_args)...);
    }
    return reinterpret_cast<MemID>(begin);
}

/**
 * @brief Destroy an array
 *
 * @tparam T The type of the array elements.
 * @param p_alloc The allocator that the array was allocated on.
 * @param p_id The memory ID of the array.
 */
template <typename T, typename AllocType>
inline void destroy_array(AllocType& p_alloc, MemID p_id, size_t p_num) {
    if (p_id == MEM_NULL) {
        return;
    }
    T* begin = static_cast<T*>(p_alloc.get(p_id));
    for (size_t i = 0; i < p_num; ++i) {
        (begin + i)->~T();
    }
    p_alloc.deallocate(p_id);
}

} // namespace WhiteBirdEngine

#endif
