#ifndef __WBE_RENDER_PASS_VK_HH__
#define __WBE_RENDER_PASS_VK_HH__

#include "core/reflection/reflection_defs.hh"
#include "function/render/render_pass.hh"
#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class RenderPassVK : public RenderPass {
public:

    struct BaseRenderDataVK {
        // The logic device.
        VkDevice logic_device;
        // The image format of the swap chain.
        VkFormat swap_chain_format;
        // The size of the swap chain.
        VkExtent2D swap_chain_extent;
        // Callback for allocator.
        VkAllocationCallbacks* allocation_callbacks;
        // Graphics queue family index.
        uint32_t queue_family_index_graphics;
        // Compute queue family index.
        uint32_t queue_family_index_compute;
    };

    /**
     * @class RenderDataVK
     * @brief Common used render data in vulkan.
     *
     */
    WBE_STRUCT(RenderDataVK, RENDER_DATA_VK) : public RenderData {
        // Used for all render passes.
        BaseRenderDataVK* base_render_data;
    };

protected:
    void create_renderable_image_2d(RenderDataVK* p_input, VkFormat p_format, VkImage* p_image);
    void create_renderable_image_view_2d(RenderDataVK* p_input, VkImage p_image, VkFormat p_format, VkImageView* p_image_view);
};

}

#endif
