#ifndef __WBE_RENDER_PASS_VK_HH__
#define __WBE_RENDER_PASS_VK_HH__

#include "core/reflection/reflection_defs.hh"
#include "function/render/render_pass.hh"
#include "function/render_vk/renderer_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class RenderPassVK : public RenderPass {
public:

    /**
     * @class RenderDataVK
     * @brief Common used render data in vulkan.
     *
     */
    WBE_STRUCT(RenderDataVK, RENDER_DATA_VK) : public RenderData {
        // Used for all render passes.
        const RendererVK::BaseRenderDataVK* base_render_data;
    };

protected:
    void create_renderable_image_2d(const RenderDataVK* p_input, VkFormat p_format, VkImage* p_image);
    void create_renderable_image_view_2d(const RenderDataVK* p_input, VkImage p_image, VkFormat p_format, VkImageView* p_image_view);
};

}

#endif
