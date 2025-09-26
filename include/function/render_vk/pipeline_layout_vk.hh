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
