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
#ifndef WBE_FILE_RO_MATERIAL_VK_HH
#define WBE_FILE_RO_MATERIAL_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ROMaterialVK
 * @brief Vulkan material object that owns descriptor bindings for one material set.
 */
class WBE_META(WBE_RENDER_OBJECT) ROMaterialVK final : public RenderObjectVK {
public:
    /**
     * @class TextureBinding
     * @brief Texture image binding keyed by shader parameter name.
     */
    struct TextureBinding {
        /**
         * @brief Name of the shader texture parameter.
         */
        std::string texture_key;
        /**
         * @brief Image view to bind for this texture parameter.
         */
        Ref<ROImageViewVK> image_view;
        /**
         * @brief Image layout used by the descriptor write.
         */
        VkImageLayout image_layout = VK_IMAGE_LAYOUT_GENERAL;
    };

    /**
     * @class SamplerBinding
     * @brief Sampler binding keyed by shader parameter name.
     */
    struct SamplerBinding {
        /**
         * @brief Name of the shader sampler parameter.
         */
        std::string sampler_key;
        /**
         * @brief Sampler to bind for this parameter.
         */
        Ref<ROSamplerVK> sampler;
    };

    /**
     * @class Builder
    * @brief Builder for ROMaterialVK.
     */
    struct Builder {
        /**
         * @brief Descriptor set layout used by this material descriptor set.
         */
        Ref<RODescriptorSetLayoutVK> descriptor_set_layout;
        /**
         * @brief Descriptor set index this material binds.
         */
        uint32_t set_index = 0;
        /**
         * @brief Descriptor bindings reflected from the pipeline shaders.
         */
        std::vector<ShaderDescriptorBindingVK> descriptor_bindings;
        /**
         * @brief Texture bindings available to match descriptor parameters.
         */
        std::vector<TextureBinding> texture_bindings;
        /**
         * @brief Sampler bindings available to match descriptor parameters.
         */
        std::vector<SamplerBinding> sampler_bindings;
        /**
         * @brief Fallback sampler when a shader sampler parameter has no explicit binding.
         */
        Ref<ROSamplerVK> default_sampler;

        /**
         * @brief Set the descriptor set layout.
         */
        Builder& set_descriptor_set_layout(Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout);
        /**
         * @brief Set the descriptor set index.
         */
        Builder& set_set_index(uint32_t p_set_index);
        /**
         * @brief Set reflected shader descriptor bindings.
         */
        Builder& set_descriptor_bindings(const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings);
        /**
         * @brief Add a texture binding keyed by shader parameter name.
         */
        Builder& add_texture_binding(const TextureBinding& p_texture_binding);
        /**
         * @brief Add a sampler binding keyed by shader parameter name.
         */
        Builder& add_sampler_binding(const SamplerBinding& p_sampler_binding);
        /**
         * @brief Set the fallback sampler.
         */
        Builder& set_default_sampler(Ref<ROSamplerVK> p_sampler);

        Ref<ROMaterialVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROMaterialVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<ROMaterialVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROMaterialVK>(p_render_data->allocator, p_render_data, *this);
        }
    };

    ROMaterialVK() = delete;
    virtual ~ROMaterialVK() override = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(ROMaterialVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Material descriptor binding configuration.
     */
    ROMaterialVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the owned descriptor set.
     */
    Ref<RODescriptorSetVK> get_descriptor_set() const {
        return descriptor_set;
    }

    /**
     * @brief Get descriptor sets in render-task binding order.
     */
    const std::vector<Ref<RODescriptorSetVK>>& get_descriptor_sets() const {
        return descriptor_sets;
    }

private:
    Ref<RODescriptorSetVK> descriptor_set;
    std::vector<Ref<RODescriptorSetVK>> descriptor_sets;

    void write_material_bindings(const Builder& p_builder);
};

} // namespace WhiteBirdEngine

#endif