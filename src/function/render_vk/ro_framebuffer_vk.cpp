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
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

VkFramebuffer ROFramebufferVK::Builder::build(const BaseRenderDataVK* p_render_data) const {
    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = render_pass->get_render_pass();
    create_info.attachmentCount = attachments.size();
    create_info.pAttachments = attachments.data();
    create_info.width = WBE_REQUIRED(width);
    create_info.height = WBE_REQUIRED(height);
    create_info.layers = layers;
    VkFramebuffer result;
    if (vkCreateFramebuffer(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create framebuffer.");
    }
    return result;
}

ROFramebufferVK::ROFramebufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : render_data(p_render_data) {
    framebuffer = p_builder.build(p_render_data);
}

ROFramebufferVK::~ROFramebufferVK() {
    vkDestroyFramebuffer(render_data->logic_device, framebuffer, render_data->allocation_callbacks);
}
}
