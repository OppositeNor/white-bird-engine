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
#ifndef WBE_FILE_STATIC_SET_NODE_CONTRACT_HH
#define WBE_FILE_STATIC_SET_NODE_CONTRACT_HH

#include <cstddef>

namespace WhiteBirdEngine {

/**
 * @class StaticSetNodeContract
 * @brief Reflected byte layout of one shader-side static-set node element.
 */
class StaticSetNodeContract final {
public:
    /**
     * @brief Construct an empty contract.
     */
    StaticSetNodeContract() = default;
    /**
     * @brief Destroy a contract.
     */
    ~StaticSetNodeContract() = default;
    /**
     * @brief Copy a contract.
     */
    StaticSetNodeContract(const StaticSetNodeContract&) = default;
    /**
     * @brief Move a contract.
     */
    StaticSetNodeContract(StaticSetNodeContract&&) noexcept = default;
    /**
     * @brief Copy a contract.
     */
    StaticSetNodeContract& operator=(const StaticSetNodeContract&) = default;
    /**
     * @brief Move a contract.
     */
    StaticSetNodeContract& operator=(StaticSetNodeContract&&) noexcept = default;

    /**
     * @brief Test exact byte-layout compatibility with another contract.
     */
    bool matches(const StaticSetNodeContract& p_other) const;

    /** Byte stride between reflected node elements. */
    size_t element_stride = 0;
    /** Byte offset of NodeData.transform. */
    size_t transform_offset = 0;
    /** Byte offset of NodeData.normal_transform. */
    size_t normal_transform_offset = 0;
};

} // namespace WhiteBirdEngine

#endif