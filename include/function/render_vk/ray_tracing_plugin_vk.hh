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
#ifndef WBE_FILE_RAY_TRACING_PLUGIN_VK_HH
#define WBE_FILE_RAY_TRACING_PLUGIN_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render_vk/render_plugin_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "utils/interface/i_singleton.hh"

#include <cstddef>
#include <span>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RayTracingPluginVK
 * @brief Vulkan renderer plugin that enables ray tracing and owns related shared resources.
 *
 * Ray-tracing state is neither copyable nor movable.
 */
class RayTracingPluginVK final : public RenderPluginVK, public ISingleton<RayTracingPluginVK> {
public:
    RayTracingPluginVK() = delete;
    virtual ~RayTracingPluginVK() override;
    RayTracingPluginVK(const RayTracingPluginVK&) = delete;
    RayTracingPluginVK(RayTracingPluginVK&&) noexcept = delete;
    RayTracingPluginVK& operator=(const RayTracingPluginVK&) = delete;
    RayTracingPluginVK& operator=(RayTracingPluginVK&&) noexcept = delete;

    /**
     * @brief Construct ray-tracing helper state.
     *
     * @param p_render_data Renderer data used for Vulkan resource creation.
     */
    explicit RayTracingPluginVK(const BaseRenderDataVK* p_render_data);

    /**
     * @brief Get the singleton ray-tracing plugin instance.
     *
     * @return The singleton instance, or null when no renderer owns the plugin.
     */
    static RayTracingPluginVK* get_singleton() {
        return singleton;
    }

    virtual bool try_enable(RendererCapabilities& p_capabilities,
        VkPhysicalDevice p_physical_device,
        const char* const* p_enabled_layers,
        size_t p_enabled_layer_count) override;

    virtual bool is_enabled() const override {
        return enabled;
    }

    virtual std::span<const char* const> get_required_instance_extensions() const override;

    virtual std::span<const char* const> get_required_device_extensions() const override;

    virtual VkBaseOutStructure* get_device_feature_chain() override;

    virtual void on_device_created(BaseRenderDataVK* p_render_data, VkPhysicalDevice p_physical_device, VkDevice p_logical_device) override;

    virtual void update_base_render_data(BaseRenderDataVK* p_render_data, VkPhysicalDevice p_physical_device) const override;
    virtual VkBufferUsageFlags get_static_opaque_vertex_buffer_usage() const override;
    virtual VkBufferUsageFlags get_static_opaque_index_buffer_usage() const override;
    virtual GPUJobVK on_static_opaque_triangles_updated(BaseRenderDataVK* p_render_data,
        const RenderDatabase::OpaqueTrianglesInfo& p_info,
        Ref<ROBufferVK> p_vertex_buffer,
        Ref<ROBufferVK> p_index_buffer,
        Ref<ROBufferVK> p_object_id_buffer) override;

    /**
     * @brief Update the static-opaque bottom-level acceleration structure for the current static mesh buffers.
     *
     * @param p_info Static-opaque mesh metadata from the render database.
     * @param p_vertex_buffer Shared static-opaque vertex buffer.
     * @param p_index_buffer Shared static-opaque index buffer.
     * @return A GPU job that records the build, or an empty job when there is no geometry.
     */
private:
    struct StaticOpaqueBuildData;
    struct StaticOpaqueTopLevelBuildData;

    const BaseRenderDataVK* render_data = nullptr;
    Ref<ROAccelerationStructureVK> static_opaque_bottom_level_acceleration_structure;
    Ref<ROAccelerationStructureVK> static_opaque_top_level_acceleration_structure;
    Ref<ROBufferVK> static_opaque_top_level_instance_buffer;
    bool enabled = false;
    VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{};

    VkDeviceAddress get_buffer_device_address(Ref<ROBufferVK> p_buffer) const;
    VkDeviceAddress get_acceleration_structure_device_address(Ref<ROAccelerationStructureVK> p_acceleration_structure) const;
    StaticOpaqueBuildData build_static_opaque_data(const RenderDatabase::OpaqueTrianglesInfo& p_info,
        VkDeviceAddress p_vertex_buffer_address,
        VkDeviceAddress p_index_buffer_address) const;
    StaticOpaqueTopLevelBuildData build_static_opaque_top_level_data(
        Ref<ROAccelerationStructureVK> p_bottom_level_acceleration_structure) const;
    GPUJobVK create_static_opaque_build_job(StaticOpaqueBuildData&& p_build_data);
    GPUJobVK update_static_opaque_acceleration_structure(
        const RenderDatabase::OpaqueTrianglesInfo& p_info, Ref<ROBufferVK> p_vertex_buffer, Ref<ROBufferVK> p_index_buffer);

    inline static RayTracingPluginVK* singleton = nullptr;
};

} // namespace WhiteBirdEngine

#endif
