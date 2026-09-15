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
#ifndef WBE_FILE_RO_ACCELERATION_STRUCTURE_VK_HH
#define WBE_FILE_RO_ACCELERATION_STRUCTURE_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "utils/utils.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ROAccelerationStructureVK
 * @brief Owns a Vulkan acceleration structure and its storage buffer.
 *
 * The owned Vulkan resources are neither copyable nor movable.
 */
class WBE_META(WBE_RENDER_OBJECT) ROAccelerationStructureVK final : public IRenderObjectVK {
public:
    ROAccelerationStructureVK() = delete;
    /**
     * @brief Create an acceleration structure with device-local storage.
     *
     * @param p_render_data Renderer data used to create and destroy Vulkan resources.
     * @param p_base_ray_tracing_data Ray-tracing procedure table owned by the active plugin.
     * @param p_size Required acceleration-structure storage size.
     * @param p_type Acceleration-structure type.
     */
    ROAccelerationStructureVK(const BaseRenderDataVK* p_render_data,
        const BaseRayTracingDataVK* p_base_ray_tracing_data,
        VkDeviceSize p_size,
        VkAccelerationStructureTypeKHR p_type);
    virtual ~ROAccelerationStructureVK() override;
    ROAccelerationStructureVK(const ROAccelerationStructureVK&) = delete;
    ROAccelerationStructureVK(ROAccelerationStructureVK&&) noexcept = delete;
    ROAccelerationStructureVK& operator=(const ROAccelerationStructureVK&) = delete;
    ROAccelerationStructureVK& operator=(ROAccelerationStructureVK&&) noexcept = delete;

    TypeID get_type_uuid() const override;

    /**
     * @brief Get the owned Vulkan acceleration-structure handle.
     */
    VkAccelerationStructureKHR get_acceleration_structure() const {
        return acceleration_structure;
    }

private:
    Ref<ROBufferVK> storage_buffer;
    const BaseRayTracingDataVK* base_ray_tracing_data = nullptr;
    VkAccelerationStructureKHR acceleration_structure = VK_NULL_HANDLE;
    PFN_vkDestroyAccelerationStructureKHR destroy_acceleration_structure = nullptr;
};

} // namespace WhiteBirdEngine

#endif
