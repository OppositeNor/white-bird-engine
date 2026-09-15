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
#ifndef WBE_FILE_RO_SAMPLER_VK_HH
#define WBE_FILE_RO_SAMPLER_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "utils/utils.hh"
#include <optional>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ROSamplerVK
 * @brief Vulkan sampler render object.
 */
class WBE_META(WBE_RENDER_OBJECT) ROSamplerVK final : public IRenderObjectVK {
public:
    /**
     * @class Builder
     * @brief Builder for ROSamplerVK.
     */
    struct Builder {
        friend ROSamplerVK;

        /**
         * @brief Magnification filter.
         */
        VkFilter mag_filter = VK_FILTER_LINEAR;
        /**
         * @brief Minification filter.
         */
        VkFilter min_filter = VK_FILTER_LINEAR;
        /**
         * @brief Mipmap sampling mode.
         */
        VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        /**
         * @brief Address mode for the U coordinate.
         */
        VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        /**
         * @brief Address mode for the V coordinate.
         */
        VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        /**
         * @brief Address mode for the W coordinate.
         */
        VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        /**
         * @brief Mipmap level-of-detail bias.
         */
        float mip_lod_bias = 0.0F;
        /**
         * @brief Whether anisotropic filtering is enabled.
         */
        VkBool32 anisotropy_enable = VK_FALSE;
        /**
         * @brief Maximum anisotropy value when anisotropy is enabled.
         */
        float max_anisotropy = 1.0F;
        /**
         * @brief Whether comparison sampling is enabled.
         */
        VkBool32 compare_enable = VK_FALSE;
        /**
         * @brief Comparison operation for comparison sampling.
         */
        VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;
        /**
         * @brief Minimum mip level of detail.
         */
        float min_lod = 0.0F;
        /**
         * @brief Maximum mip level of detail.
         */
        float max_lod = VK_LOD_CLAMP_NONE;
        /**
         * @brief Border color used by clamp-to-border modes.
         */
        VkBorderColor border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        /**
         * @brief Whether coordinates are unnormalized.
         */
        VkBool32 unnormalized_coordinates = VK_FALSE;
        std::optional<uint32_t> sampler_index;

        /**
         * @brief Set minification and magnification filters.
         */
        Builder& set_filter(VkFilter p_min_filter, VkFilter p_mag_filter);
        /**
         * @brief Set magnification filter.
         */
        Builder& set_mag_filter(VkFilter p_mag_filter);
        /**
         * @brief Set minification filter.
         */
        Builder& set_min_filter(VkFilter p_min_filter);
        /**
         * @brief Set mipmap sampling mode.
         */
        Builder& set_mipmap_mode(VkSamplerMipmapMode p_mipmap_mode);
        /**
         * @brief Set one address mode for all texture coordinates.
         */
        Builder& set_address_mode(VkSamplerAddressMode p_address_mode);
        /**
         * @brief Set address modes for U, V, and W texture coordinates.
         */
        Builder& set_address_mode(
            VkSamplerAddressMode p_address_mode_u, VkSamplerAddressMode p_address_mode_v, VkSamplerAddressMode p_address_mode_w);
        /**
         * @brief Set address mode for the U coordinate.
         */
        Builder& set_address_mode_u(VkSamplerAddressMode p_address_mode_u);
        /**
         * @brief Set address mode for the V coordinate.
         */
        Builder& set_address_mode_v(VkSamplerAddressMode p_address_mode_v);
        /**
         * @brief Set address mode for the W coordinate.
         */
        Builder& set_address_mode_w(VkSamplerAddressMode p_address_mode_w);
        /**
         * @brief Set mipmap level-of-detail bias.
         */
        Builder& set_mip_lod_bias(float p_mip_lod_bias);
        /**
         * @brief Set anisotropy enable state and maximum anisotropy.
         */
        Builder& set_anisotropy(VkBool32 p_anisotropy_enable, float p_max_anisotropy);
        /**
         * @brief Set whether anisotropic filtering is enabled.
         */
        Builder& set_anisotropy_enable(VkBool32 p_anisotropy_enable);
        /**
         * @brief Set maximum anisotropy.
         */
        Builder& set_max_anisotropy(float p_max_anisotropy);
        /**
         * @brief Set comparison sampling enable state and operation.
         */
        Builder& set_compare(VkBool32 p_compare_enable, VkCompareOp p_compare_op);
        /**
         * @brief Set whether comparison sampling is enabled.
         */
        Builder& set_compare_enable(VkBool32 p_compare_enable);
        /**
         * @brief Set comparison operation.
         */
        Builder& set_compare_op(VkCompareOp p_compare_op);
        /**
         * @brief Set minimum and maximum mip level-of-detail range.
         */
        Builder& set_lod_range(float p_min_lod, float p_max_lod);
        /**
         * @brief Set minimum mip level of detail.
         */
        Builder& set_min_lod(float p_min_lod);
        /**
         * @brief Set maximum mip level of detail.
         */
        Builder& set_max_lod(float p_max_lod);
        /**
         * @brief Set border color.
         */
        Builder& set_border_color(VkBorderColor p_border_color);
        /**
         * @brief Set whether texture coordinates are unnormalized.
         */
        Builder& set_unnormalized_coordinates(VkBool32 p_unnormalized_coordinates);
        /**
         * @brief Transfer an allocated sampler descriptor index to the created sampler.
         *
         * The created resource writes the descriptor and becomes responsible for releasing the index. The caller must not
         * release the index after construction succeeds.
         */
        Builder& set_sampler_index(uint32_t p_index);

        Ref<ROSamplerVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROSamplerVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<ROSamplerVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROSamplerVK>(p_render_data->allocator, p_render_data, *this);
        }

    private:
        VkSampler create_vk_object(const BaseRenderDataVK* p_render_data) const;
    };

    ROSamplerVK() = delete;
    virtual ~ROSamplerVK() override;
    ROSamplerVK(const ROSamplerVK&) = delete;
    ROSamplerVK(ROSamplerVK&&) noexcept = delete;
    ROSamplerVK& operator=(const ROSamplerVK&) = delete;
    ROSamplerVK& operator=(ROSamplerVK&&) noexcept = delete;

    TypeID get_type_uuid() const override;

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Sampler configuration builder.
     */
    ROSamplerVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the Vulkan sampler handle.
     */
    VkSampler get_sampler() const {
        return sampler;
    }

    /**
     * @brief Get or lazily create the sampler descriptor index.
     */
    uint32_t get_sampler_index() const;

private:
    VkSampler sampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo create_info{};
    mutable std::optional<uint32_t> sampler_index;
};

} // namespace WhiteBirdEngine

#endif
