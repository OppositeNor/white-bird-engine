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
#ifndef __WBE_PIPELINE_LAYOUT_VK_HH__
#define __WBE_PIPELINE_LAYOUT_VK_HH__

#include "function/render_vk/renderer_vk.hh"
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class PipelineLayoutVK
 * @brief Pipeline layout.
 *
 */
class PipelineLayoutVK {
public:

    // Pipeline layout configurations.
    struct PipelineLayoutConfiguration {
        // Descriptor sets.
        std::vector<VkDescriptorSetLayout> descriptor_sets;
    };

    PipelineLayoutVK(const RendererVK::BaseRenderDataVK* p_render_data);
    ~PipelineLayoutVK();

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
};

}

#endif
