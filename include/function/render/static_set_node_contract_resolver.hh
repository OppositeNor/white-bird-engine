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
#ifndef WBE_FILE_STATIC_SET_NODE_CONTRACT_RESOLVER_HH
#define WBE_FILE_STATIC_SET_NODE_CONTRACT_RESOLVER_HH

#include "function/render/shader_metadata.hh"
#include "function/render/static_set_node_contract.hh"
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class StaticSetNodeContractResolver
 * @brief Resolves the reflected byte layout of the shader-side static-set node buffer.
 */
class StaticSetNodeContractResolver final {
public:
    /**
     * @brief Construct a stateless resolver.
     */
    StaticSetNodeContractResolver() = default;
    /**
     * @brief Destroy a resolver.
     */
    ~StaticSetNodeContractResolver() = default;
    /**
     * @brief Copy a resolver.
     */
    StaticSetNodeContractResolver(const StaticSetNodeContractResolver&) = default;
    /**
     * @brief Move a resolver.
     */
    StaticSetNodeContractResolver(StaticSetNodeContractResolver&&) noexcept = default;
    /**
     * @brief Copy a resolver.
     */
    StaticSetNodeContractResolver& operator=(const StaticSetNodeContractResolver&) = default;
    /**
     * @brief Move a resolver.
     */
    StaticSetNodeContractResolver& operator=(StaticSetNodeContractResolver&&) noexcept = default;

    /**
     * @brief Resolve the shader-side byte layout of one `NodeData` element.
     * @throws std::runtime_error When the reflection metadata is missing or malformed.
     */
    StaticSetNodeContract resolve(const ShaderMetadata& p_metadata, std::string_view p_static_set_parameter_name) const;
};

} // namespace WhiteBirdEngine

#endif