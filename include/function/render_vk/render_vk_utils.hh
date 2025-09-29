#ifndef __WBE_RENDER_VK_UTILS_HH__
#define __WBE_RENDER_VK_UTILS_HH__

#include "function/render/renderer.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct BaseRenderDataVK {
    // The render allocator.
    RenderAllocator* allocator;
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
    // The viewport for the final presentation.
    VkViewport viewport;
    // The scissor for the final presentation.
    VkRect2D scissor;
};


}

#endif
