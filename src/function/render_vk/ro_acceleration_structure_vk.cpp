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
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include <format>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TypeID ROAccelerationStructureVK::get_type_uuid() const {
    return TypeUUID<ROAccelerationStructureVK>::UUID;
}

ROAccelerationStructureVK::ROAccelerationStructureVK(
    const BaseRenderDataVK* p_render_data, VkDeviceSize p_size, VkAccelerationStructureTypeKHR p_type)
    : RenderObjectVK(p_render_data),
      storage_buffer(ROBufferVK::Builder()
              .set_domain(DeviceDomain::DEVICE)
              .set_size(p_size)
              .set_usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
              .create_ref(p_render_data)) {
    VkAccelerationStructureCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.buffer = storage_buffer->get_buffer();
    create_info.size = p_size;
    create_info.type = p_type;
    VkResult result = render_data->create_acceleration_structure(
        render_data->logic_device, &create_info, render_data->allocation_callbacks, &acceleration_structure);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create Vulkan acceleration structure: {}.", string_VkResult(result)));
    }
}

ROAccelerationStructureVK::~ROAccelerationStructureVK() {
    render_data->register_delayed_call(
        [render_data = render_data, acceleration_structure = acceleration_structure]() {
            render_data->destroy_acceleration_structure(render_data->logic_device, acceleration_structure, render_data->allocation_callbacks);
        },
        render_data->frames_in_flight);
}

} // namespace WhiteBirdEngine
