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
#ifndef WBE_FILE_SUBOBJECT_OFFSET_HH
#define WBE_FILE_SUBOBJECT_OFFSET_HH

#include <cstddef>
#include <cstdint>

namespace WhiteBirdEngine {

/**
 * @brief Get the byte offset from one address to another.
 *
 * Memory handles keep the memory ID of the whole allocation and the byte offset of the subobject they view, so that a
 * handle converted to a base that is not at offset 0 (a non-primary base) still addresses that base.
 *
 * @param p_from The address the offset starts from.
 * @param p_to The address the offset leads to.
 * @return The offset in bytes, negative if p_to precedes p_from.
 */
template <typename From, typename To>
std::ptrdiff_t get_address_offset(From* p_from, To* p_to) {
    return static_cast<std::ptrdiff_t>(reinterpret_cast<std::uintptr_t>(p_to) - reinterpret_cast<std::uintptr_t>(p_from));
}

/**
 * @brief Get the pointer to a subobject of an allocation.
 *
 * @tparam T The type of the subobject.
 * @param p_allocation The start of the allocation. May be nullptr, which yields nullptr.
 * @param p_offset The byte offset of the subobject from the start of the allocation.
 * @return The pointer to the subobject.
 */
template <typename T>
T* apply_subobject_offset(void* p_allocation, std::ptrdiff_t p_offset) {
    if (p_allocation == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<T*>(static_cast<std::byte*>(p_allocation) + p_offset);
}

/**
 * @brief Get the subobject offset of a handle after converting its object pointer to another type.
 *
 * @tparam To The type the handle is converted to.
 * @tparam From The type the handle currently views.
 * @param p_object The object the handle currently views. May be nullptr, which keeps the offset.
 * @param p_offset The handle's current subobject offset.
 * @return The subobject offset of the converted handle.
 */
template <typename To, typename From>
std::ptrdiff_t get_converted_subobject_offset(From* p_object, std::ptrdiff_t p_offset) {
    if (p_object == nullptr) {
        return p_offset;
    }
    To* converted = static_cast<To*>(p_object);
    return p_offset + get_address_offset(p_object, converted);
}

} // namespace WhiteBirdEngine

#endif
