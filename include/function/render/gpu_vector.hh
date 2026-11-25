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
#ifndef WBE_FILE_GPU_VECTOR_HH
#define WBE_FILE_GPU_VECTOR_HH

#include "core/memory/reference_strong.hh"
#include "function/render/ro_buffer.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class GPUVector
 * @brief A size-tracked array of elements stored in a buffer, with its size stored in the buffer too.
 *
 * The buffer starts with a HEADER_SIZE-byte header whose first uint32_t is the byte size of the elements, so shaders can read
 * the size; the elements follow the header. This is the layout of the shader-side GPUVector in utils/gpu_vector.slang. The
 * number of elements is the byte size divided by sizeof(T), rounded down. The capacity is the number of whole elements after
 * the header, and never grows. Elements are never destroyed, so the element type must be trivially copyable.
 *
 * The vector reads and writes the header and the elements through the buffer's host mapping. Over a buffer that is not host
 * visible, the size and elements are unavailable to the CPU: every operation that needs them fails with NOT_VISIBLE, and
 * only capacity() and get_buffer() work.
 *
 * The vector is a contiguous range: begin() and end() are pointers into the mapping, so it works with range-for,
 * std::ranges algorithms, and std::span. end() reads the size when it is called; an iteration does not see elements added
 * or removed after it.
 *
 * Movable, not copyable; a moved-from vector holds no buffer and must only be destroyed or assigned. Confined to the
 * rendering thread, like its buffer.
 *
 * @tparam T The element type.
 */
template <typename T>
class GPUVector {
    static_assert(std::is_trivially_copyable_v<T>, "GPUVector elements must be trivially copyable.");

public:
    /**
     * @brief Size in bytes of the header before the elements. The header's first uint32_t is the byte size of the
     * elements; the rest is padding that keeps the elements 16-byte aligned.
     */
    static constexpr size_t HEADER_SIZE = 16;

    static_assert(alignof(T) <= HEADER_SIZE, "GPUVector elements must not need more than 16-byte alignment.");

    /**
     * @brief The element type.
     */
    using value_type = T; // NOLINT
    /**
     * @brief Mutable iterator: a pointer into the buffer's mapping.
     */
    using iterator = T*; // NOLINT
    /**
     * @brief Read-only iterator: a pointer into the buffer's mapping.
     */
    using const_iterator = const T*; // NOLINT

    /**
     * @enum Failure
     * @brief The outcome of a try_ operation.
     */
    enum class Failure {
        OK,
        UNKNOWN,
        BUFFER_OVERFLOW,
        INDEX_OVERFLOW,
        NOT_VISIBLE,
    };

    GPUVector() = delete;

    /**
     * @brief Constructor. Writes the byte size of the initial elements into the buffer's header.
     *
     * @param p_buffer The buffer the size and elements are stored in.
     * @param p_initial_size The initial number of elements, which keep the buffer's current contents.
     * @throws std::runtime_error If the buffer is null, smaller than the header, has room for more element bytes than
     * uint32_t counts, or the initial size exceeds the capacity or is not zero for a buffer that is not host visible.
     */
    GPUVector(Ref<ROBuffer> p_buffer, uint32_t p_initial_size = 0)
        : buffer(std::move(p_buffer)) {
        if (buffer == MEM_NULL) {
            throw std::runtime_error("Failed to create GPU vector: buffer is null.");
        }
        if (buffer->get_size() < HEADER_SIZE) {
            throw std::runtime_error(std::format("Failed to create GPU vector: buffer of {} bytes is smaller than the {}-byte header.",
                                                 buffer->get_size(), HEADER_SIZE));
        }
        if ((buffer->get_size() - HEADER_SIZE) / sizeof(T) * sizeof(T) > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error(std::format(
                "Failed to create GPU vector: buffer of {} bytes has room for more element bytes than uint32_t counts.", buffer->get_size()));
        }
        if (p_initial_size > capacity()) {
            throw std::runtime_error(std::format(
                "Failed to create GPU vector: initial size {} exceeds capacity {}.", p_initial_size, capacity()));
        }
        auto* mapped = static_cast<std::byte*>(buffer->get_mapped());
        if (mapped == nullptr) {
            if (p_initial_size != 0) {
                throw std::runtime_error(std::format(
                    "Failed to create GPU vector: cannot record initial size {} in a buffer that is not host visible.", p_initial_size));
            }
            return;
        }
        byte_size = reinterpret_cast<uint32_t*>(mapped);
        data = reinterpret_cast<T*>(mapped + HEADER_SIZE);
        store_size(p_initial_size);
    }

    ~GPUVector() = default;
    GPUVector(const GPUVector&) = delete;
    GPUVector& operator=(const GPUVector&) = delete;

    GPUVector(GPUVector&& p_other) noexcept
        : buffer(std::move(p_other.buffer)), byte_size(std::exchange(p_other.byte_size, std::nullopt)),
          data(std::exchange(p_other.data, nullptr)) {
    }

    GPUVector& operator=(GPUVector&& p_other) noexcept {
        if (this != &p_other) {
            buffer = std::move(p_other.buffer);
            byte_size = std::exchange(p_other.byte_size, std::nullopt);
            data = std::exchange(p_other.data, nullptr);
        }
        return *this;
    }

    /**
     * @brief Get the number of elements if the buffer is host visible.
     *
     * @return The byte size stored in the buffer's header divided by sizeof(T), rounded down, or NOT_VISIBLE if the
     * buffer is not host visible.
     */
    [[nodiscard]] std::expected<uint32_t, Failure> try_size() const {
        if (!byte_size.has_value()) {
            return std::unexpected(Failure::NOT_VISIBLE);
        }
        return static_cast<uint32_t>(**byte_size / sizeof(T));
    }

    /**
     * @brief Get the number of elements.
     *
     * @return The byte size stored in the buffer's header divided by sizeof(T), rounded down.
     * @throws std::runtime_error If the buffer is not host visible.
     */
    uint32_t size() const {
        std::expected<uint32_t, Failure> result = try_size();
        if (result.has_value()) {
            return result.value();
        }
        if (result.error() == Failure::NOT_VISIBLE) {
            throw std::runtime_error("Failed to get the size of GPU vector: the buffer is not host visible.");
        }
        throw std::runtime_error("Failed to get the size of GPU vector: unknown error.");
    }

    /**
     * @brief Get the number of whole elements the buffer holds after the header.
     *
     * @return The capacity.
     */
    uint32_t capacity() const {
        return static_cast<uint32_t>((buffer->get_size() - HEADER_SIZE) / sizeof(T));
    }

    /**
     * @brief Append an element.
     *
     * @param p_element The element.
     * @throws std::runtime_error If the vector is full or the buffer is not host visible.
     */
    template <typename T1>
    void push_back(T1&& p_element) {
        switch (try_push_back(std::forward<T1>(p_element))) {
            case Failure::OK:
                return;
            case Failure::BUFFER_OVERFLOW:
                throw std::runtime_error(std::format("Failed to push back element: the vector is full at capacity {}.", capacity()));
            case Failure::NOT_VISIBLE:
                throw std::runtime_error("Failed to push back element: the buffer is not host visible.");
            default:
                throw std::runtime_error("Failed to push back element: unknown error.");
        }
    }

    /**
     * @brief Append an element if there is room.
     *
     * @param p_element The element.
     * @return OK, BUFFER_OVERFLOW if the vector is full, or NOT_VISIBLE if the buffer is not host visible. The vector is
     * unchanged on failure.
     */
    template <typename T1>
    [[nodiscard]] Failure try_push_back(T1&& p_element) {
        if (!byte_size.has_value()) {
            return Failure::NOT_VISIBLE;
        }
        const uint32_t current_size = size();
        if (current_size >= capacity()) {
            return Failure::BUFFER_OVERFLOW;
        }
        new (data + current_size) T(std::forward<T1>(p_element));
        store_size(current_size + 1);
        return Failure::OK;
    }

    /**
     * @brief Set the number of elements. Elements added by growing keep the buffer's current contents.
     *
     * @param p_new_size The new size.
     * @throws std::runtime_error If the new size exceeds the capacity or the buffer is not host visible.
     */
    void resize(uint32_t p_new_size) {
        switch (try_resize(p_new_size)) {
            case Failure::OK:
                return;
            case Failure::BUFFER_OVERFLOW:
                throw std::runtime_error(std::format("Failed to resize vector: size: {} is larger than the vector's capacity: {}",
                                                     p_new_size, capacity()));
            case Failure::NOT_VISIBLE:
                throw std::runtime_error("Failed to resize vector: the buffer is not host visible.");
            default:
                throw std::runtime_error("Failed to resize vector: unknown error.");
        }
    }

    /**
     * @brief Set the number of elements if it fits.
     *
     * @param p_new_size The new size.
     * @return OK, BUFFER_OVERFLOW if the new size exceeds the capacity, or NOT_VISIBLE if the buffer is not host visible.
     * The size is unchanged on failure.
     */
    [[nodiscard]] Failure try_resize(uint32_t p_new_size) {
        if (!byte_size.has_value()) {
            return Failure::NOT_VISIBLE;
        }
        if (p_new_size > capacity()) {
            return Failure::BUFFER_OVERFLOW;
        }
        store_size(p_new_size);
        return Failure::OK;
    }

    /**
     * @brief Get an element.
     *
     * @param p_index The index.
     * @return The element in the buffer's mapping.
     * @throws std::runtime_error If the index is out of range or the buffer is not host visible.
     */
    T& get(uint32_t p_index) {
        std::expected<T*, Failure> result = try_get(p_index);
        if (result.has_value()) {
            return *result.value();
        }
        switch (result.error()) {
            case Failure::INDEX_OVERFLOW:
                throw std::runtime_error(std::format("Failed to get value at index: {}, index overflowed. Vector size: {}", p_index, size()));
            case Failure::NOT_VISIBLE:
                throw std::runtime_error(std::format("Failed to get value at index: {}, the buffer is not host visible.", p_index));
            default:
                throw std::runtime_error(std::format("Failed to get value at index: {}, unknown error.", p_index));
        }
    }

    /**
     * @brief Get an element if it exists.
     *
     * @param p_index The index.
     * @return The element's address in the buffer's mapping, INDEX_OVERFLOW if the index is out of range, or NOT_VISIBLE
     * if the buffer is not host visible.
     */
    [[nodiscard]] std::expected<T*, Failure> try_get(uint32_t p_index) {
        if (!byte_size.has_value()) {
            return std::unexpected(Failure::NOT_VISIBLE);
        }
        if (p_index >= size()) {
            return std::unexpected(Failure::INDEX_OVERFLOW);
        }
        return data + p_index;
    }

    /**
     * @brief Get an iterator to the first element.
     *
     * @return The first element's address in the buffer's mapping.
     * @throws std::runtime_error If the buffer is not host visible.
     */
    iterator begin() {
        return require_data();
    }

    /**
     * @brief Get an iterator past the last element.
     *
     * @return The address after the last element in the buffer's mapping.
     * @throws std::runtime_error If the buffer is not host visible.
     */
    iterator end() {
        return require_data() + size();
    }

    /**
     * @brief Get an iterator to the first element.
     *
     * @return The first element's address in the buffer's mapping.
     * @throws std::runtime_error If the buffer is not host visible.
     */
    const_iterator begin() const {
        return require_data();
    }

    /**
     * @brief Get an iterator past the last element.
     *
     * @return The address after the last element in the buffer's mapping.
     * @throws std::runtime_error If the buffer is not host visible.
     */
    const_iterator end() const {
        return require_data() + size();
    }

    /**
     * @brief Get the buffer the size and elements are stored in.
     *
     * @return The buffer.
     */
    Ref<ROBuffer> get_buffer() {
        return buffer;
    }

    /**
     * @brief Get the buffer the size and elements are stored in.
     *
     * @return The buffer.
     */
    Ref<const ROBuffer> get_buffer() const {
        return buffer;
    }

private:
    T* require_data() const {
        if (!byte_size.has_value()) {
            throw std::runtime_error("Failed to iterate GPU vector: the buffer is not host visible.");
        }
        return data;
    }

    // Requires a host-visible buffer and a size within the capacity, whose byte size therefore fits in uint32_t.
    void store_size(uint32_t p_size) {
        WBE_DEBUG_ASSERT(byte_size.has_value());
        WBE_DEBUG_ASSERT(p_size <= capacity());
        **byte_size = static_cast<uint32_t>(p_size * sizeof(T));
    }

    Ref<ROBuffer> buffer = MEM_NULL;
    // The byte size of the elements in the buffer's header, or nullopt when the buffer is not host visible.
    std::optional<uint32_t*> byte_size;
    // The first element in the buffer's mapping, or null when the buffer is not host visible.
    T* data = nullptr;
};

} // namespace WhiteBirdEngine

#endif
