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
#ifndef __WBE_RENDER_VK_UTILS_HH__
#define __WBE_RENDER_VK_UTILS_HH__

#include "function/render/render_database.hh"
#include "function/render/renderer.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class BaseRenderDataVK
 * @brief The base render data that is commonly used throughout
 * the render objects.
 *
 */
struct BaseRenderDataVK {
    // The render allocator.
    RenderAllocator* allocator;
    // The render database.
    RenderDatabase<RenderAllocator>* render_database;

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

    // Graphics command pool.
    VkCommandPool graphics_command_pool;
    // Compute command pool.
    VkCommandPool compute_command_pool;
};


}

#endif
