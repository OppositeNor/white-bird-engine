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
#include "function/render_vk/render_object_vk.hh"
#include "function/function_utils.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

VkDeviceMemory RenderObjectVK::allocate_device_memory(
    VkMemoryRequirements p_memory_requirements, DeviceDomain p_domain) const {
    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = p_memory_requirements.size;
    allocate_info.memoryTypeIndex = render_data->find_memory_type(
        p_memory_requirements.memoryTypeBits, p_domain);
    VkDeviceMemory result = VK_NULL_HANDLE;
    if (vkAllocateMemory(render_data->logic_device,
            &allocate_info,
            render_data->allocation_callbacks,
            &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate memory.");
    }
    return result;
}

} // namespace WhiteBirdEngine
