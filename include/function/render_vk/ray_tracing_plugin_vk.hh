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

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/render_plugin_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/interface/i_singleton.hh"

#include <cstddef>
#include <span>
#include <vector>
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

    virtual void on_before_instance_creation(VkInstanceCreateInfo* p_create_info) override;
    virtual void on_after_instance_created(VkInstance p_instance) override;
    virtual void on_before_device_creation(
        VkPhysicalDevice p_physical_device, VkDeviceCreateInfo* p_create_info, VkPhysicalDeviceFeatures* p_device_features) override;

    virtual void on_after_device_created(
        BaseRenderDataVK* p_render_data, VkPhysicalDevice p_physical_device, VkDevice p_logical_device) override;

    virtual void update_base_render_data(BaseRenderDataVK* p_render_data, VkPhysicalDevice p_physical_device) override;
    virtual void configure_descriptor_heaps(
        const BaseRenderDataVK* p_render_data, std::span<DescriptorHeapConfigurationVK> p_configurations) override;
    virtual void create_descriptor_arenas(BaseRenderDataVK* p_render_data) override;
    virtual StaticGeometryBufferUsageVK get_static_geometry_buffer_usage() const override;
    virtual GPUJobVK on_static_opaque_set_updated(BaseRenderDataVK* p_render_data,
        RefRes<RUStaticOpaqueSet> p_set,
        Ref<ROBufferVK> p_vertex_buffer,
        Ref<ROBufferVK> p_index_buffer,
        RefRes<RUStaticOpaqueDoubleSidedSet> p_double_sided_set = MEM_NULL) override;

    /**
     * @brief Get the prepared static-opaque top-level acceleration structure.
     *
     * @return The stable top-level acceleration structure render object, or null when no static-opaque geometry is available.
     */
    Ref<ROAccelerationStructureVK> get_static_opaque_top_level_acceleration_structure() const {
        return static_opaque_top_level_acceleration_structure;
    }

    /**
     * @brief Get the stable container bound to the current static-opaque TLAS.
     *
     * @return The stable container used by graph consumers, or null when no static-opaque TLAS has been prepared.
     */
    Ref<ROCSingleVK> get_static_opaque_top_level_acceleration_structure_container() const {
        return static_opaque_top_level_acceleration_structure_container;
    }

    /**
     * @brief Update the static-opaque bottom-level acceleration structure for the current static mesh buffers.
     *
     * @param p_set Static-opaque geometry set published by the scene.
     * @param p_vertex_buffer Shared static-opaque vertex buffer.
     * @param p_index_buffer Shared static-opaque index buffer.
     * @return A GPU job that records the build, or an empty job when there is no geometry.
     */
private:
    struct StaticOpaqueBottomLevelBuild;
    struct StaticOpaqueBuildData;
    struct StaticOpaqueTopLevelBuild;

    const BaseRenderDataVK* render_data = nullptr;
    VkInstance vk_instance = VK_NULL_HANDLE;
    VkDeviceSize acceleration_structure_descriptor_byte_offset = 0;
    Unique<BaseRayTracingDataVK> base_ray_tracing_data;
    std::vector<Ref<ROAccelerationStructureVK>> static_opaque_bottom_level_acceleration_structures;
    Ref<ROAccelerationStructureVK> static_opaque_top_level_acceleration_structure;
    Ref<ROCSingleVK> static_opaque_top_level_acceleration_structure_container;
    Ref<ROBufferVK> static_opaque_top_level_instance_buffer;
    bool enabled = false;
    bool ray_query_enabled = false;
    bool ray_tracing_pipeline_enabled = false;
    VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{};
    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{};

    bool supports_base_features(VkPhysicalDevice p_physical_device, const char* const* p_enabled_layers, size_t p_enabled_layer_count) const;
    bool supports_ray_query(VkPhysicalDevice p_physical_device, const char* const* p_enabled_layers, size_t p_enabled_layer_count) const;
    bool supports_ray_tracing_pipeline(
        VkPhysicalDevice p_physical_device, const char* const* p_enabled_layers, size_t p_enabled_layer_count) const;
    void prepare_device_features(const RendererCapabilities& p_capabilities);
    void reset_static_opaque_acceleration_structures();
    void validate_static_opaque_set(RefRes<RUStaticGeometrySet> p_set) const;
    void validate_static_opaque_submesh(const RUStaticGeometrySubmesh& p_submesh, size_t p_instance_count) const;
    const RUVertexView::Attribute& get_static_opaque_position_attribute(const RUStaticGeometrySubmesh& p_submesh) const;
    StaticOpaqueBottomLevelBuild create_static_opaque_bottom_level_build(const RUStaticGeometrySubmesh& p_submesh,
        RefRes<RUStaticGeometrySet> p_set,
        VkDeviceAddress p_vertex_buffer_address,
        VkDeviceAddress p_index_buffer_address) const;
    Ref<ROBufferVK> create_aligned_scratch_buffer(
        VkDeviceSize p_required_size, VkDeviceSize p_alignment, VkDeviceAddress& p_aligned_buffer_address) const;
    void initialize_bottom_level_builds(StaticOpaqueBuildData& p_build_data,
        std::vector<Ref<ROAccelerationStructureVK>>& p_bottom_level_acceleration_structures,
        VkDeviceSize& p_max_bottom_level_scratch_size,
        size_t& p_total_instance_count) const;
    StaticOpaqueTopLevelBuild create_top_level_build(StaticOpaqueBuildData& p_build_data, VkDeviceSize p_scratch_alignment) const;
    StaticOpaqueBuildData build_static_opaque_data(
        RefRes<RUStaticGeometrySet> p_set, VkDeviceAddress p_vertex_buffer_address, VkDeviceAddress p_index_buffer_address) const;
    GPUJobVK create_static_opaque_record_job(std::vector<Ref<ROAccelerationStructureVK>>&& p_bottom_level_acceleration_structures,
        Ref<ROBufferVK> p_bottom_level_scratch_buffer,
        StaticOpaqueBuildData&& p_build_data,
        VkDeviceAddress p_aligned_bottom_level_scratch_buffer_address,
        StaticOpaqueTopLevelBuild&& p_top_level_build) const;
    GPUJobVK create_static_opaque_build_job(StaticOpaqueBuildData&& p_build_data);
    GPUJobVK update_static_opaque_acceleration_structure(RefRes<RUStaticGeometrySet> p_set,
        Ref<ROBufferVK> p_vertex_buffer,
        Ref<ROBufferVK> p_index_buffer,
        RefRes<RUStaticOpaqueDoubleSidedSet> p_double_sided_set);

    inline static RayTracingPluginVK* singleton = nullptr;
};

} // namespace WhiteBirdEngine

#endif
