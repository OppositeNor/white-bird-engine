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
#ifndef WBE_FILE_STATIC_SET_COMPILER_VK_HH
#define WBE_FILE_STATIC_SET_COMPILER_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/static_set_resources_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <optional>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

class RendererVK;

/**
 * @class StaticSetCompilerVK
 * @brief Compiles static geometry into pipeline-independent Vulkan resources.
 */
class StaticSetCompilerVK final {
public:
    /**
     * @class Config
     * @brief Material-contract configuration used during resource compilation.
     */
    struct Config {
        Config() = default;
        ~Config() = default;
        Config(const Config&) = default;
        Config(Config&&) noexcept = default;
        Config& operator=(const Config&) = default;
        Config& operator=(Config&&) noexcept = default;

        /** Default material resource used to fill missing shader texture roles. */
        std::string default_material_resource_key = "default_material";
        /** Reflected material parameter-buffer base name. */
        std::string material_params_variable_name = "material_params";
    };

    StaticSetCompilerVK() = delete;
    ~StaticSetCompilerVK() = default;
    StaticSetCompilerVK(const StaticSetCompilerVK&) = delete;
    StaticSetCompilerVK(StaticSetCompilerVK&&) noexcept = delete;
    StaticSetCompilerVK& operator=(const StaticSetCompilerVK&) = delete;
    StaticSetCompilerVK& operator=(StaticSetCompilerVK&&) noexcept = delete;

    /** @brief Construct a compiler using renderer-owned Vulkan services. */
    explicit StaticSetCompilerVK(RendererVK* p_renderer);

    /**
     * @brief Compile one resource set containing opaque and masked draw batches.
     *
     * Vertex metadata supplies layout and static-node contracts only. The result owns no pipeline or render-task behavior.
     * Null opaque or masked sets are allowed, but at least one non-empty set is required. The returned object is neither
     * copyable nor movable and must be shared through Ref.
     *
     * @throws std::runtime_error If resource data or reflected contracts are invalid or incompatible.
     */
    Ref<StaticSetResourcesVK> compile(RefRes<RUStaticOpaqueSet> p_opaque_set,
        RefRes<RUStaticMaskedSet> p_masked_set,
        const ShaderMetadata& p_opaque_vertex_metadata,
        std::string_view p_opaque_vertex_entry,
        const ShaderMetadata& p_masked_vertex_metadata,
        std::string_view p_masked_vertex_entry,
        const std::optional<MaterialTextureContract>& p_material_contract,
        const Config& p_config) const;

private:
    RendererVK* renderer;
};

} // namespace WhiteBirdEngine

#endif
