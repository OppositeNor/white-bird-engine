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
#ifndef WBE_FILE_RO_SHADER_VK_HH
#define WBE_FILE_RO_SHADER_VK_HH

#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/render/i_rend_object.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class ROShaderVK;

/**
 * @struct ShaderDescVK
 * @brief Resource creation description; copyable and movable.
 * Copying duplicates the owned SPIR-V words of every variant; moving transfers them. Source resources need not outlive
 * this description.
 */
struct ShaderDescVK {
    friend ROShaderVK;
    /**
     * @class Variant
     * @brief SPIR-V words of one variant.
     */
    struct Variant {
        /**
         * @brief The variant's name.
         */
        std::string name;
        /**
         * @brief The variant's SPIR-V words.
         */
        std::vector<uint32_t> code;
    };
    /**
     * @brief The index of the default variant in variants.
     */
    size_t default_variant_index = 0;
    /**
     * @brief The variants, sorted by unique name.
     */
    Required<std::vector<Variant>> variants;

    /**
     * @brief Set the source code of a shader with one variant.
     */
    ShaderDescVK& set_code(std::span<const uint32_t> p_code);

    /**
     * @brief Set every variant from a resource ID.
     */
    ShaderDescVK& set_code(std::string_view p_resource_id);

    /**
     * @brief Set every variant from a resource.
     */
    ShaderDescVK& set_code(RefRes<RUShader> p_resource);

    /**
     * @brief Create a reference-counted resource on the rendering thread.
     * @param p_rend_context Context that outlives the created resource.
     */
    Ref<ROShaderVK> create_ref(const RendContextVK* p_rend_context) const;

    /**
     * @brief Create a uniquely owned resource on the rendering thread.
     * @param p_rend_context Context that outlives the created resource.
     */
    Unique<ROShaderVK> create_unique(const RendContextVK* p_rend_context) const;

private:
    static VkShaderModule create_vk_object(const RendContextVK* p_rend_data, const Variant& p_variant);
};

/**
 * @class ROShaderVK
 * @brief Shader class for Vulkan. Holds one shader module per variant of its source, all created eagerly.
 *
 */
class WBE_META(WBE_REND_OBJECT) ROShaderVK final : public IRendObject {
public:
    /**
     * @brief Compatibility name for the resource creation description.
     */
    using Builder = ShaderDescVK;

    ROShaderVK() = delete;
    virtual ~ROShaderVK() override;
    ROShaderVK(const ROShaderVK&) = delete;
    ROShaderVK(ROShaderVK&&) = delete;
    ROShaderVK& operator=(const ROShaderVK&) = delete;
    ROShaderVK& operator=(ROShaderVK&&) = delete;

    virtual TypeID get_type_uuid() const override;

    /**
     * @brief Constructor.
     *
     * @param p_rend_context The base render data.
     * @param p_builder Configuration builder.
     */
    ROShaderVK(const RendContextVK* p_rend_context, const Builder& p_builder);
    /**
     * @brief Constructor from resource ID.
     *
     * @param p_rend_context The base render data.
     * @param p_resource_id The resource ID.
     */
    ROShaderVK(const RendContextVK* p_rend_context, std::string_view p_resource_id);
    /**
     * @brief Constructor from a shader resource.
     *
     * @param p_rend_context The base render data.
     * @param p_resource_id The shader's resource.
     */
    ROShaderVK(const RendContextVK* p_rend_context, RefRes<RUShader> p_resource);

    /**
     * @brief Get the shader module of the default variant; same as get_variant({}).
     *
     * @return The shader module.
     */
    VkShaderModule get_shader_module() const;

    /**
     * @brief Get the shader module of a variant by name, by binary search.
     *
     * @param p_name The variant's name; empty selects the default variant.
     * @return The variant's shader module, owned by this object.
     * @throws std::runtime_error When no variant has the name.
     */
    VkShaderModule get_variant(std::string_view p_name = {}) const;

protected:

    virtual void apply_debug_name(std::string_view p_name) override;

private:
    struct VariantModule {
        std::string name;
        VkShaderModule module = VK_NULL_HANDLE;
    };
    const RendContextVK* rend_data = nullptr;
    size_t default_variant_index = 0;
    // Sorted by name.
    std::vector<VariantModule> variant_modules;

    void init(const Builder& p_builder);
};
} // namespace WhiteBirdEngine

#endif
