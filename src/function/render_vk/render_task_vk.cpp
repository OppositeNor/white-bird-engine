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
#include "function/render_vk/render_task_vk.hh"
#include "core/logging/log.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RenderTaskVK::RecordCommandBufferGuard::RecordCommandBufferGuard(VkCommandBuffer p_command_buffer,
                                                                 VkCommandBufferInheritanceInfo* p_inheritance_info)
: command_buffer(p_command_buffer) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = p_inheritance_info;
    if (vkBeginCommandBuffer(p_command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer.");
    }
}

RenderTaskVK::RecordCommandBufferGuard::~RecordCommandBufferGuard() {
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        // Destructor cannot throw an exception. 
        stdout_log()->error("Failed to record command buffer.");
    }
}

} // namespace WhiteBirdEngine
