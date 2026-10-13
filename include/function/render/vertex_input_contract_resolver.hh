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
#ifndef WBE_FILE_VERTEX_INPUT_CONTRACT_RESOLVER_HH
#define WBE_FILE_VERTEX_INPUT_CONTRACT_RESOLVER_HH

#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/vertex_input_contract.hh"
#include <span>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class VertexInputContractResolver
 * @brief Validates and matches reflected vertex roles against resource layouts.
 */
class VertexInputContractResolver final {
public:
    /**
     * @brief Construct a stateless resolver.
     */
    VertexInputContractResolver() = default;
    /**
     * @brief Destroy a resolver.
     */
    ~VertexInputContractResolver() = default;
    /**
     * @brief Copy a resolver.
     */
    VertexInputContractResolver(const VertexInputContractResolver&) = default;
    /**
     * @brief Move a resolver.
     */
    VertexInputContractResolver(VertexInputContractResolver&&) noexcept = default;
    /**
     * @brief Copy a resolver.
     */
    VertexInputContractResolver& operator=(const VertexInputContractResolver&) = default;
    /**
     * @brief Move a resolver.
     */
    VertexInputContractResolver& operator=(VertexInputContractResolver&&) noexcept = default;

    /**
     * @brief Resolve a vertex shader entry against the vertex bindings of a submesh.
     *
     * Each shader role must be declared by exactly one binding. Only the bindings the shader reads appear in the contract,
     * in binding order.
     * @throws std::runtime_error When shader annotations or a binding layout are malformed or incompatible, or a role is
     * missing or declared by more than one binding.
     */
    VertexInputContract resolve(
        const ShaderMetadata& p_metadata, std::string_view p_entry_name, std::span<const RUVertexView> p_vertex_bindings) const;
};

} // namespace WhiteBirdEngine

#endif
