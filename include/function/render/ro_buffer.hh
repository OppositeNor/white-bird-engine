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
#ifndef WBE_FILE_RO_BUFFER_HH
#define WBE_FILE_RO_BUFFER_HH

#include "function/render/i_rend_object.hh"
#include <cstddef>

namespace WhiteBirdEngine {

/**
 * @class ROBuffer
 * @brief Backend-independent buffer render object. Neither copyable nor movable.
 *
 * Confined to the rendering thread, like every render object.
 */
class ROBuffer : public IRendObject {
public:
    ROBuffer() = default;
    virtual ~ROBuffer() override = default;
    ROBuffer(const ROBuffer&) = delete;
    ROBuffer(ROBuffer&&) noexcept = delete;
    ROBuffer& operator=(const ROBuffer&) = delete;
    ROBuffer& operator=(ROBuffer&&) noexcept = delete;

    /**
     * @brief Get the size of the buffer.
     *
     * @return The size in bytes.
     */
    virtual size_t get_size() const = 0;

    /**
     * @brief Get the host mapping of the buffer's first byte.
     *
     * The mapping is persistent and lives as long as the buffer.
     *
     * @return The mapped address, or null if the buffer's memory is not host visible.
     */
    virtual void* get_mapped() const = 0;

    /**
     * @brief Copy host bytes into the buffer and make them visible to the device.
     *
     * @param p_data The bytes to copy.
     * @param p_dst_offset Offset in the buffer the first byte is copied to.
     * @param p_size Number of bytes to copy.
     * @throws std::runtime_error If the buffer is not host visible or the region exceeds the buffer.
     */
    virtual void update_buffer(const void* p_data, size_t p_dst_offset, size_t p_size) = 0;
};

} // namespace WhiteBirdEngine

#endif
