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
#ifndef WBE_FILE_RAY_TRACING_VK_HH
#define WBE_FILE_RAY_TRACING_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/render_database.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"

#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RayTracingVK
 * @brief Owns Vulkan ray-tracing resource preparation that is shared by render tasks.
 *
 * Ray-tracing state is neither copyable nor movable.
 */
class RayTracingVK final {
public:
    RayTracingVK() = delete;
    ~RayTracingVK() = default;
    RayTracingVK(const RayTracingVK&) = delete;
    RayTracingVK(RayTracingVK&&) noexcept = delete;
    RayTracingVK& operator=(const RayTracingVK&) = delete;
    RayTracingVK& operator=(RayTracingVK&&) noexcept = delete;

    /**
     * @brief Construct ray-tracing helper state.
     *
     * @param p_render_data Renderer data used for Vulkan resource creation.
     */
    explicit RayTracingVK(const BaseRenderDataVK* p_render_data);

    /**
     * @brief Update the static-opaque bottom-level acceleration structure for the current static mesh buffers.
     *
     * @param p_info Static-opaque mesh metadata from the render database.
     * @param p_vertex_buffer Shared static-opaque vertex buffer.
     * @param p_index_buffer Shared static-opaque index buffer.
     * @return A GPU job that records the build, or an empty job when there is no geometry.
     */
    GPUJobVK update_static_opaque_acceleration_structure(
        const RenderDatabase::OpaqueTrianglesInfo& p_info, Ref<ROBufferVK> p_vertex_buffer, Ref<ROBufferVK> p_index_buffer);

    /**
     * @brief Get the current static-opaque bottom-level acceleration-structure handle.
     */
    VkAccelerationStructureKHR get_static_opaque_acceleration_structure() const {
        return static_opaque_acceleration_structure == MEM_NULL
                   ? VK_NULL_HANDLE
                   : static_opaque_acceleration_structure->get_acceleration_structure();
    }

private:
    struct StaticOpaqueBuildData;

    const BaseRenderDataVK* render_data = nullptr;
    Ref<ROAccelerationStructureVK> static_opaque_acceleration_structure;

    VkDeviceAddress get_buffer_device_address(Ref<ROBufferVK> p_buffer) const;
    StaticOpaqueBuildData build_static_opaque_data(const RenderDatabase::OpaqueTrianglesInfo& p_info,
        VkDeviceAddress p_vertex_buffer_address,
        VkDeviceAddress p_index_buffer_address) const;
    GPUJobVK create_static_opaque_build_job(StaticOpaqueBuildData&& p_build_data);
};

} // namespace WhiteBirdEngine

#endif
