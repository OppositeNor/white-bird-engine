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
#ifndef __WBE_RO_PIPELINE_LAYOUT_VK_HH__
#define __WBE_RO_PIPELINE_LAYOUT_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROPipelineLayoutVK
 * @brief Pipeline layout.
 *
 */
class ROPipelineLayoutVK final : public RenderObject {
public:
    // Builder for ROPipelineLayoutVK
    struct Builder {
        friend ROPipelineLayoutVK;
        // Descriptor set layouts.
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
        // Push constant ranges.
        std::vector<VkPushConstantRange> push_constant_ranges;

        // Set descriptor set layouts.
        Builder& set_descriptor_set_layouts(const std::vector<VkDescriptorSetLayout>& p_descriptor_set_layouts);
        // Add a single descriptor set layout.
        Builder& add_descriptor_set_layout(VkDescriptorSetLayout p_descriptor_set_layout);
        // Set push constant ranges.
        Builder& set_push_constant_ranges(const std::vector<VkPushConstantRange>& p_push_constant_ranges);
        // Add a single push constant range.
        Builder& add_push_constant_range(const VkPushConstantRange& p_push_constant_range);

    private:
        VkPipelineLayout build(const RendererVK::BaseRenderDataVK* p_render_data) const;
    };

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    ROPipelineLayoutVK(const RendererVK::BaseRenderDataVK* p_render_data, const Builder& p_builder);
    virtual ~ROPipelineLayoutVK() override;

    virtual std::string get_ro_type() const override {
        return "pipeline_layout_vk";
    }

    /**
     * @brief Get the vulkan pipeline layout instance.
     *
     * @return The vulkan pipeline layout instance.
     */
    VkPipelineLayout get_pipeline_layout() const {
        return pipeline_layout;
    }
private:
    const RendererVK::BaseRenderDataVK* render_data;
    VkPipelineLayout pipeline_layout;

    void init(const Builder& p_builder);
};

}

#endif
