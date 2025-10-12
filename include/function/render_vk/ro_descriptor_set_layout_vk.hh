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
#ifndef __WBE_RO_DESCRIPTOR_SET_LAYOUT_VK_HH__
#define __WBE_RO_DESCRIPTOR_SET_LAYOUT_VK_HH__

#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RODescriptorSetLayoutVK
 * @brief Descriptor set layout.
 *
 */
class RODescriptorSetLayoutVK : public RenderObjectVK {
public:
    // Builder for RODescriptorSetLayoutVK
    struct Builder {
        friend RODescriptorSetLayoutVK;
        // The descriptor set bindings.
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        // Set the descriptor set bindings.
        Builder& set_bindings(const std::vector<VkDescriptorSetLayoutBinding>& p_bindings);
        // Add a single binding.
        Builder& add_binding(const VkDescriptorSetLayoutBinding& p_binding);

    private:
        VkDescriptorSetLayout build(const BaseRenderDataVK* p_render_data) const;
    };

    ~RODescriptorSetLayoutVK();
    RODescriptorSetLayoutVK(const RODescriptorSetLayoutVK&) = delete;
    RODescriptorSetLayoutVK(RODescriptorSetLayoutVK&&) = delete;
    RODescriptorSetLayoutVK& operator=(const RODescriptorSetLayoutVK&) = delete;
    RODescriptorSetLayoutVK& operator=(RODescriptorSetLayoutVK&&) = delete;

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
    virtual std::string get_ro_type() const override {
        return "descriptor_set_layout_vk";
    }

    /**
     * @brief Get the vulkan descriptor set layout instance.
     *
     * @return The vulkan descriptor set layout instance.
     */
    VkDescriptorSetLayout get_descriptor_set_layout() const {
        return descriptor_set_layout;
    }

private:
    VkDescriptorSetLayout descriptor_set_layout;

    void init(const Builder& p_builder);
};

}

#endif
