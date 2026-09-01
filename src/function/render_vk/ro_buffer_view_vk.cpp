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

#include "function/render_vk/ro_buffer_view_vk.hh"
#include "core/core_utils.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TypeID ROBufferViewVK::get_type_uuid() const {
    return TypeUUID<ROBufferViewVK>::UUID;
}

VkBufferView ROBufferViewVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
    VkBufferViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    create_info.buffer = WBE_REQUIRED(buffer).get_buffer();
    create_info.format = WBE_REQUIRED(format);
    create_info.offset = offset;
    create_info.range = range;
    VkBufferView result;
    if (vkCreateBufferView(p_render_data->logic_device, &create_info, p_render_data->allocation_callbacks, &result) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer view.");
    }
    return result;
}

ROBufferViewVK::ROBufferViewVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data) {
    buffer_view = p_builder.create_vk_object(p_render_data);
    exposed_fields.buffer = p_builder.buffer;
}

ROBufferViewVK::~ROBufferViewVK() {
    // Deallocate resource after frames in flight.
    render_data->register_delayed_call(
        [render_data = render_data, buffer_view = buffer_view]() {
            vkDestroyBufferView(render_data->logic_device, buffer_view, render_data->allocation_callbacks);
        },
        render_data->frames_in_flight);
}
} // namespace WhiteBirdEngine
