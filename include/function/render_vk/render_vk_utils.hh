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
#include <cstdint>
#include <functional>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class BaseRenderDataVK
 * @brief The base render data that is commonly used throughout
 * the render objects.
 *
 */
struct BaseRenderDataVK {
    BaseRenderDataVK(RenderAllocator* p_allocator)
        : queue_family_indices(p_allocator), pre_execute_tasks_queue(p_allocator) {}
    // The render allocator.
    RenderAllocator* allocator = nullptr;
    // The render database.
    RenderDatabase* render_database = nullptr;

    // The logic device.
    VkDevice logic_device = VK_NULL_HANDLE;
    // The image format of the swap chain.
    VkFormat swap_chain_format = VK_FORMAT_UNDEFINED;
    // The size of the swap chain.
    VkExtent2D swap_chain_extent{};
    // Callback for allocator.
    VkAllocationCallbacks* allocation_callbacks = nullptr;
    // Graphics queue family.
    VkQueue graphics_queue = VK_NULL_HANDLE;
    // Compute queue family.
    VkQueue compute_queue = VK_NULL_HANDLE;
    // The viewport for the final presentation.
    VkViewport viewport{};
    // The scissor for the final presentation.
    VkRect2D scissor{};

    // Graphics command pool.
    VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
    // Compute command pool.
    VkCommandPool compute_command_pool = VK_NULL_HANDLE;

    // The fence that blocks every render tick.
    VkFence in_flight_fence = VK_NULL_HANDLE;
    // The current swap chain index.
    uint32_t current_swap_chain_index = 0;

    // The current number of frames.
    uint64_t current_frame = 0;
    // The maximum depth of the render graph in the current pipeline.
    uint64_t max_depth = 0;

    // The queue family indices.
    Set<uint32_t> queue_family_indices;

    // Task to execute in the pre-execution task.
    Vector<std::function<void(VkCommandBuffer)>> pre_execute_tasks_queue;

    // A function that finds a suitable memory type for a certain memory property.
    std::function<uint32_t(uint32_t, VkMemoryPropertyFlags)> find_memory_type;

};

} // namespace WhiteBirdEngine

#endif
