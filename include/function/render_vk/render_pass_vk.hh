#ifndef __WBE_RENDER_PASS_VK_HH__
#define __WBE_RENDER_PASS_VK_HH__

#include "core/reflection/reflection_defs.hh"
#include "function/render/render_pass.hh"
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
        VkDevice logic_device;
        VkFormat swap_chain_format;
        VkExtent2D swap_chain_extent;
        VkAllocationCallbacks* allocation_callbacks;
    };
};

}

#endif
