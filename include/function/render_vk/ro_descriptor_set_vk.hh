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
#ifndef WBE_RO_DESCRIPTOR_SET_HH
#define WBE_RO_DESCRIPTOR_SET_HH

#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RODescriptorSetVK
 * @brief Descriptor set
 *
 */
class WBE_META(WBE_RENDER_OBJECT) RODescriptorSetVK final : public RenderObjectVK {
public:
    /**
     * @class Builder
     * @brief The builder for RODescriptorSetVK.
     *
     */
    struct Builder {
        friend RODescriptorSetVK;
        /**
         * @brief The descriptor set layouts.
         */
        std::vector<Ref<RODescriptorSetLayoutVK>> descriptor_set_layouts;

        /**
         * @brief Add a descriptor set layout.
         *
         * @param p_descriptor_set_layout The descriptor set layout to add.
         * @return Builder& Reference to this builder.
         */
        Builder& add_descriptor_set_layout(const Ref<RODescriptorSetLayoutVK>& p_descriptor_set_layout) {
            descriptor_set_layouts.push_back(p_descriptor_set_layout);
            return *this;
        }

    private:
        VkDescriptorSet build(const BaseRenderDataVK* p_render_data) const;
    };

    RODescriptorSetVK() = delete;
    virtual ~RODescriptorSetVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RODescriptorSetVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    RODescriptorSetVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

private:
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    Vector<Ref<RODescriptorSetLayoutVK>> descriptor_set_layouts;
};

} // namespace WhiteBirdEngine

#endif
