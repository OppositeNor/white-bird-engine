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
#ifndef WBE_FILE_RO_DESCRIPTOR_SET_LAYOUT_VK_HH
#define WBE_FILE_RO_DESCRIPTOR_SET_LAYOUT_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RODescriptorSetLayoutVK
 * @brief Descriptor set layout.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) RODescriptorSetLayoutVK final : public RenderObjectVK {
public:
    // Builder for RODescriptorSetLayoutVK
    struct Builder {
        friend RODescriptorSetLayoutVK;
        // The descriptor set bindings.
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        // Descriptor indexing flags aligned with bindings.
        std::vector<VkDescriptorBindingFlags> binding_flags;

        /**
         * @brief Set the bindings.
         *
         * @param p_bindings The bindings to set to.
         * @return *this.
         */
        Builder& set_bindings(const std::span<VkDescriptorSetLayoutBinding>& p_bindings);
        /**
         * @brief Add a binding.
         *
         * @param p_binding The binding to add.
         * @return *this.
         */
        Builder& add_binding(const VkDescriptorSetLayoutBinding& p_binding);
        /**
         * @brief Add a binding with descriptor indexing flags.
         *
         * @param p_binding The binding to add.
         * @param p_binding_flags Descriptor indexing behavior for the binding.
         * @return *this.
         */
        Builder& add_binding(const VkDescriptorSetLayoutBinding& p_binding, VkDescriptorBindingFlags p_binding_flags);

        /**
         * @brief Add a uniform-buffer binding.
         *
         * @param p_binding The binding number of this entry.
         * @param p_descriptor_count The number of descriptors contained in this
         * binding.
         * @param p_shader_stages The shader stages that could access this
         * binding.
         * @return *this.
         */
        Builder& add_ubo(uint32_t p_binding, uint32_t p_descriptor_count, VkShaderStageFlags p_shader_stages = VK_SHADER_STAGE_ALL);

        Ref<RODescriptorSetLayoutVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<RODescriptorSetLayoutVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<RODescriptorSetLayoutVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<RODescriptorSetLayoutVK>(p_render_data->allocator, p_render_data, *this);
        }

    private:
        VkDescriptorSetLayout create_vk_object(const BaseRenderDataVK* p_render_data) const;
    };

    ~RODescriptorSetLayoutVK();
    RODescriptorSetLayoutVK(const RODescriptorSetLayoutVK&) = delete;
    RODescriptorSetLayoutVK(RODescriptorSetLayoutVK&&) noexcept = delete;
    RODescriptorSetLayoutVK& operator=(const RODescriptorSetLayoutVK&) = delete;
    RODescriptorSetLayoutVK& operator=(RODescriptorSetLayoutVK&&) noexcept = delete;

    TypeID get_type_uuid() const override;

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    RODescriptorSetLayoutVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the vulkan descriptor set layout instance.
     *
     * @return The vulkan descriptor set layout instance.
     */
    VkDescriptorSetLayout get_descriptor_set_layout() const {
        return descriptor_set_layout;
    }

    /**
     * @brief Get the upper bound declared for this layout's variable-count binding.
     *
     * @return The upper bound, or zero when the layout has no variable-count binding.
     */
    uint32_t get_variable_descriptor_count_upper_bound() const {
        return variable_descriptor_count_upper_bound;
    }

private:
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    uint32_t variable_descriptor_count_upper_bound = 0;

    void init(const Builder& p_builder);
};

} // namespace WhiteBirdEngine

#endif
