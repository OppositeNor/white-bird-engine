#ifndef __WBE_ARRAY_LIST_HH__
#define __WBE_ARRAY_LIST_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "global/stl_allocator.hh"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <shared_mutex>
namespace WhiteBirdEngine {

/**
 * @class AtomicArrayList
 * @brief Atomic encapsolation of the std::vector container.
 *
 * @tparam T The type of the data.
 * @tparam Allocator The allocator.
 */
template <typename T, typename Allocator>
class AtomicArrayList {
public:
    ~AtomicArrayList() {}
    AtomicArrayList(const AtomicArrayList& p_other) {
        std::unique_lock lock(p_other.mutex);
        data = p_other.data;
    }
    AtomicArrayList(AtomicArrayList&& p_other) {
        std::unique_lock lock(p_other.mutex);
        data = std::move(p_other.data);
    }
    AtomicArrayList& operator=(const AtomicArrayList& p_other) {
        if (&p_other == this) {
            return *this;
        }
        std::unique_lock lock_other(p_other.mutex);
        std::unique_lock lock(mutex);
        data = p_other.data;
        return *this;
    }
    AtomicArrayList& operator=(AtomicArrayList&& p_other) {
        if (&p_other == this) {
            return *this;
        }
        std::unique_lock lock_other(p_other.mutex);
        std::unique_lock lock(mutex);
        data = std::move(p_other.data);
        return *this;
    }

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator for the array list.
     */
    AtomicArrayList(Allocator* p_allocator)
        : data(p_allocator) {}
    /**
     * @brief Consstructor.
     *
     * @param p_array The vector array to initialize the values.
     */
    AtomicArrayList(const vectorp<T>& p_array)
        : data(p_array) {}
    /**
     * @brief Constructor.
     *
     * @param p_array The vector array to initialize the values.
     */
    AtomicArrayList(vectorp<T>&& p_array)
        : data(std::move(p_array)) {}
    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator for the array list.
     * @param p_array The vector array to initialize the values.
     */
    AtomicArrayList(Allocator* p_allocator, const vectorp<T>& p_array)
        : data(p_array.begin(), p_array.end(), p_allocator) {}


    /**
     * @brief Fetch data to a buffer.
     *
     * @param p_buffer The buffer to fetch to.
     * @param p_buffer_size The size of the buffer to prevent buffer overflow.
     */
    void fetch(T* p_buffer, size_t p_buffer_size) {
        std::shared_lock lock(mutex);
        memcpy(p_buffer, data.data(), std::max(p_buffer_size, data.size()) * sizeof(T));
    }

    /**
     * @brief Push a value to the end of the array list.
     *
     * @param p_value The value to be pushed.
     */
    void push_back(const T& p_value) {
        std::unique_lock lock(mutex);
        data.push_back(p_value);
    }

    /**
     * @brief Push a value to the end of the array list.
     *
     * @param p_value The value to be pushed.
     */
    void push_back(T&& p_value) {
        std::unique_lock lock(mutex);
        data.push_back(std::move(p_value));
    }

    /**
     * @brief Set a value at a specific index.
     *
     * @param p_index The index to set the value to.
     * @param p_value The value to be set.
     */
    void set(int p_index, const T& p_value) {
        std::unique_lock lock(mutex);
        unguard_set(p_index, p_value);
    }

    /**
     * @brief Set a value at a specific index.
     *
     * @param p_index The index to set the value to.
     * @param p_value The value to be set.
     */
    void set(int p_index, T&& p_value) {
        std::unique_lock lock(mutex);
        unguard_set(p_index, std::move(p_value));
    }

    /**
     * @brief Get the copy of a value at a specific index.
     *
     * @param p_index The index to get the value from.
     * @return The value of the array at the index.
     */
    T get(int p_index) const {
        std::shared_lock lock(mutex);
        return unguard_get(p_index);
    }

    /**
     * @brief Get the size of the array list.
     *
     * @return The size of the array list.
     */
    size_t size() const {
        std::shared_lock lock(mutex);
        return unguard_size();
    }

    /**
     * @brief Set a value at a specific index. This function is not
     * atomic. If you want atomic set, use the set method instead.
     *
     * @param p_index The index of the array to set the value.
     * @param p_value The value to be set to.
     */
    void unguard_set(int p_index, const T& p_value) {
        data[p_index] = p_value;
    }

    /**
     * @brief Set a value at a specific index. This function is not
     * atomic. If you want atomic set, use the set method instead.
     *
     * @param p_index The index of the array to set the value.
     * @param p_value The value to be set to.
     */
    void unguard_set(int p_index, T&& p_value) {
        data[p_index] = std::move(p_value);
    }

    /**
     * @brief Get the value at a index. This funciton is not
     * atomic. If you want atomic get, use the get method instead.
     *
     * @param p_index The index of the value to get from.
     * @return The value of the array at the index.
     */
    const T& unguard_get(int p_index) const {
        return data[p_index];
    }

    /**
     * @brief Get the value at a index. This funciton is not
     * atomic. If you want atomic get, use the get method instead.
     *
     * @param p_index The index of the value to get from.
     * @return The value of the array at the index.
     */
    T& unguard_get(int p_index) {
        return data[p_index];
    }

    /**
     * @brief Get the size of the array list.
     *
     * @return The size of the array list.
     */
    size_t unguard_size() const {
        return data.size();
    }
private:
    mutable std::shared_mutex mutex;
    vectorp<T> data;

};

template <typename T>
using AtomicArrayListP = AtomicArrayList<T, HeapAllocatorAlignedPool>;


}

#endif
