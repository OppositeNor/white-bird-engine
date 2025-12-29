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
#ifndef WBE_FILE_RO_PIPELINE_LAYOUT_VK_HH
#define WBE_FILE_RO_PIPELINE_LAYOUT_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "utils/defs.hh"
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROPipelineLayoutVK
 * @brief Pipeline layout.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) ROPipelineLayoutVK final : public RenderObjectVK {
public:
    // Builder for ROPipelineLayoutVK
    struct Builder {
        friend ROPipelineLayoutVK;
        // Descriptor set layouts.
        std::vector<Ref<RODescriptorSetLayoutVK>> descriptor_set_layouts;
        // Push constant ranges.
        std::vector<VkPushConstantRange> push_constant_ranges;

        // Set descriptor set layouts.
        Builder& set_descriptor_set_layouts(const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts);
        // Add a single descriptor set layout.
        Builder& add_descriptor_set_layout(Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout);
        // Set push constant ranges.
        Builder& set_push_constant_ranges(const std::vector<VkPushConstantRange>& p_push_constant_ranges);
        // Add a single push constant range.
        Builder& add_push_constant_range(const VkPushConstantRange& p_push_constant_range);

    private:
        VkPipelineLayout build(const BaseRenderDataVK* p_render_data) const;
    };
    ROPipelineLayoutVK() = delete;
    virtual ~ROPipelineLayoutVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(ROPipelineLayoutVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    ROPipelineLayoutVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the vulkan pipeline layout instance.
     *
     * @return The vulkan pipeline layout instance.
     */
    VkPipelineLayout get_pipeline_layout() const {
        return pipeline_layout;
    }

    /**
     * @class ExposedFields
     * @brief The exposed fields of this render objects.
     *
     */
    struct ExposedFields {
        ExposedFields(const BaseRenderDataVK* p_render_data)
            : descriptor_set_layouts(p_render_data->allocator) {}
        Vector<Ref<RODescriptorSetLayoutVK>> descriptor_set_layouts;
    };

    const ExposedFields& fields() const {
        return exposed_fields;
    }
private:
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    ExposedFields exposed_fields;

    void init(const Builder& p_builder);
};
} // namespace WhiteBirdEngine

#endif
