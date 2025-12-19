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
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RORenderPassVK::Builder& RORenderPassVK::Builder::set_attachments(const std::vector<VkAttachmentDescription>& p_attachments) {
    attachments = p_attachments;
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::add_attachment(const VkAttachmentDescription& p_attachment) {
    attachments.push_back(p_attachment);
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::set_subpasses(const std::vector<VkSubpassDescription>& p_subpasses) {
    subpasses = p_subpasses;
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::add_subpass(const VkSubpassDescription& p_subpass) {
    subpasses.push_back(p_subpass);
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::set_dependencies(const std::vector<VkSubpassDependency>& p_dependencies) {
    dependencies = p_dependencies;
    return *this;
}

RORenderPassVK::Builder& RORenderPassVK::Builder::add_dependency(const VkSubpassDependency& p_dependency) {
    dependencies.push_back(p_dependency);
    return *this;
}

VkRenderPass RORenderPassVK::Builder::build(const BaseRenderDataVK* p_render_data) const {
    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = attachments.size();
    create_info.pAttachments = attachments.data();
    create_info.subpassCount = subpasses.size();
    create_info.pSubpasses = subpasses.data();
    create_info.dependencyCount = dependencies.size();
    create_info.pDependencies = dependencies.data();
    
    VkRenderPass result;
    if (vkCreateRenderPass(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
    return result;
}

RORenderPassVK::RORenderPassVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    init(p_builder);
}

RORenderPassVK::~RORenderPassVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call([render_data = render_data, render_pass = render_pass]() {
        vkDestroyRenderPass(render_data->logic_device, render_pass, render_data->allocation_callbacks);
    }, render_data->frames_in_flight);
}

void RORenderPassVK::init(const Builder& p_builder) {
    render_pass = p_builder.build(render_data);
}

} // namespace WhiteBirdEngine
