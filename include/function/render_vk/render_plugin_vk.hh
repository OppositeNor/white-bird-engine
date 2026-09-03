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
#ifndef WBE_FILE_RENDER_PLUGIN_VK_HH
#define WBE_FILE_RENDER_PLUGIN_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_vk_defs.hh"

#include <cstddef>
#include <span>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class ROBufferVK;

/**
 * @class RenderPluginVK
 * @brief Vulkan renderer plugin interface for optional device capabilities.
 *
 * Implementations are owned by the renderer, are neither copyable nor movable,
 * and may append required extensions and feature chains when enabled on the
 * chosen physical device.
 */
class RenderPluginVK {
public:
    RenderPluginVK() = default;
    virtual ~RenderPluginVK() = default;
    RenderPluginVK(const RenderPluginVK&) = delete;
    RenderPluginVK(RenderPluginVK&&) noexcept = delete;
    RenderPluginVK& operator=(const RenderPluginVK&) = delete;
    RenderPluginVK& operator=(RenderPluginVK&&) noexcept = delete;

    /**
     * @brief Probe the selected physical device and enable the plugin when possible.
     *
     * @param p_capabilities Renderer capabilities requested by the caller. Implementations may disable unsupported capabilities in-place.
     * @param p_physical_device Selected physical device.
     * @param p_enabled_layers Enabled Vulkan layers.
     * @param p_enabled_layer_count Number of enabled Vulkan layers.
     * @return True when the plugin was enabled for device creation.
     */
    virtual bool try_enable(RendererCapabilities& p_capabilities,
        VkPhysicalDevice p_physical_device,
        const char* const* p_enabled_layers,
        size_t p_enabled_layer_count) = 0;

    /**
     * @brief Whether this plugin is currently enabled.
     *
     * @return True when try_enable() successfully activated the plugin.
     */
    virtual bool is_enabled() const = 0;

    /**
     * @brief Get the instance extensions required by this plugin.
     *
     * @return Plugin-owned extension names. Returns an empty span when none are required.
     */
    virtual std::span<const char* const> get_required_instance_extensions() const = 0;

    /**
     * @brief Get the device extensions required by this plugin when enabled.
     *
     * @return Plugin-owned extension names. Returns an empty span when disabled.
     */
    virtual std::span<const char* const> get_required_device_extensions() const = 0;

    /**
     * @brief Get the head of the plugin-owned device-feature pNext chain.
     *
     * @return The first feature structure to append, or null when none.
     */
    virtual VkBaseOutStructure* get_device_feature_chain() = 0;

    /**
     * @brief Finish plugin setup after the logical device has been created.
     *
     * @param p_render_data Renderer data to populate with plugin-owned Vulkan commands or limits.
     * @param p_physical_device Selected physical device.
     * @param p_logical_device Created logical device.
     */
    virtual void on_device_created(BaseRenderDataVK* p_render_data, VkPhysicalDevice p_physical_device, VkDevice p_logical_device) = 0;

    /**
     * @brief Refresh plugin-owned physical-device properties stored in the base render data.
     *
     * @param p_render_data Renderer data to update.
     * @param p_physical_device Selected physical device.
     */
    virtual void update_base_render_data(BaseRenderDataVK* p_render_data, VkPhysicalDevice p_physical_device) const = 0;

    /**
     * @brief Add extra usage bits required by this plugin for the shared static-opaque vertex buffer.
     *
     * @return Additional Vulkan buffer-usage flags. Returns zero when none are required.
     */
    virtual VkBufferUsageFlags get_static_opaque_vertex_buffer_usage() const = 0;

    /**
     * @brief Add extra usage bits required by this plugin for the shared static-opaque index buffer.
     *
     * @return Additional Vulkan buffer-usage flags. Returns zero when none are required.
     */
    virtual VkBufferUsageFlags get_static_opaque_index_buffer_usage() const = 0;

    /**
     * @brief React to refreshed static-opaque mesh buffers.
     *
     * @param p_render_data Renderer state shared with render objects.
     * @param p_info Refreshed render-database geometry metadata.
     * @param p_vertex_buffer Shared static-opaque vertex buffer.
     * @param p_index_buffer Shared static-opaque index buffer.
     * @param p_object_id_buffer Shared static-opaque object-id buffer.
     * @return An optional GPU job to enqueue after the shared buffers are updated.
     */
    virtual GPUJobVK on_static_triangle_set_updated(BaseRenderDataVK* p_render_data,
        const RenderDatabase::OpaqueTrianglesInfo& p_info,
        Ref<ROBufferVK> p_vertex_buffer,
        Ref<ROBufferVK> p_index_buffer,
        Ref<ROBufferVK> p_object_id_buffer) = 0;
};

/**
 * @brief Register built-in Vulkan render plugins requested by the renderer.
 *
 * @param p_registered_plugins Output plugin list owned through the abstract interface.
 * @param p_allocator Allocator used for plugin instances.
 * @param p_render_data Renderer base state shared with plugins.
 * @param p_capabilities Requested renderer capabilities.
 */
void register_builtin_render_plugins_vk(Vector<Ref<RenderPluginVK>, RenderAllocator>& p_registered_plugins,
    RenderAllocator* p_allocator,
    const BaseRenderDataVK* p_render_data,
    const RendererCapabilities& p_capabilities);

} // namespace WhiteBirdEngine

#endif
