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
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "resource/resource_unit/resource_unit.hh"

#include <cstddef>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class ROBufferVK;
class RendererVK;

/**
 * @class StaticGeometryBufferUsageVK
 * @brief Additional Vulkan usage bits required for shared static-geometry vertex and index buffers.
 */
struct StaticGeometryBufferUsageVK {
    VkBufferUsageFlags vertex_buffer_usage = 0;
    VkBufferUsageFlags index_buffer_usage = 0;
};

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
     * @brief Update instance creation immediately before vkCreateInstance.
     *
     * @param p_create_info Mutable instance create info whose referenced storage remains valid through creation.
     */
    virtual void on_before_instance_creation(VkInstanceCreateInfo* p_create_info) = 0;

    /**
     * @brief Update device creation immediately before vkCreateDevice.
     *
     * Implementations use enable_device_feature() to merge feature requirements without duplicating structure types.
     *
     * @param p_physical_device Physical device whose feature support is queried.
     * @param p_create_info Mutable device create info whose referenced storage remains valid through creation.
     */
    virtual void on_before_device_creation(VkPhysicalDevice p_physical_device, VkDeviceCreateInfo* p_create_info) = 0;

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
     * @brief Add extra usage bits required by this plugin for shared static-geometry buffers.
     */
    virtual StaticGeometryBufferUsageVK get_static_geometry_buffer_usage() const = 0;

    /**
     * @brief React to an explicit static opaque set update.
     *
     * @param p_render_data Renderer state shared with render objects.
     * @param p_set Published static opaque set, or null to clear derived state.
     * @param p_vertex_buffer Shared static-opaque vertex buffer.
     * @param p_index_buffer Shared static-opaque index buffer.
     * @return An optional GPU job to enqueue after the shared buffers are updated.
     */
    virtual GPUJobVK on_static_opaque_set_updated(BaseRenderDataVK* p_render_data,
        RefRes<RUStaticOpaqueSet> p_set,
        Ref<ROBufferVK> p_vertex_buffer,
        Ref<ROBufferVK> p_index_buffer) = 0;

protected:
    friend class RendererVK;

    /**
     * @brief Check one Boolean Vulkan device feature through a features2 query.
     *
     * @tparam StructureType Structure type identifying FeatureType.
     * @tparam FeatureType Vulkan feature structure containing the requested member.
     * @param p_physical_device Physical device to query.
     * @param p_feature Boolean feature member to inspect.
     * @return True when the feature is supported.
     */
    template <VkStructureType StructureType, typename FeatureType>
    static bool check_device_feature_support(VkPhysicalDevice p_physical_device, VkBool32 FeatureType::* p_feature) {
        FeatureType feature{};
        feature.sType = StructureType;
        VkPhysicalDeviceFeatures2 device_features{};
        device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        device_features.pNext = &feature;
        vkGetPhysicalDeviceFeatures2(p_physical_device, &device_features);
        return feature.*p_feature == VK_TRUE;
    }

    /**
     * @brief Enable one supported Boolean device feature without duplicating its structure type.
     *
     * The method searches p_create_info by StructureType. It sets the requested member on an existing structure or appends
     * p_fallback_feature when absent.
     *
     * @tparam StructureType Structure type identifying FeatureType.
     * @tparam FeatureType Vulkan feature structure containing the requested member.
     * @param p_physical_device Physical device whose support is required.
     * @param p_create_info Device create info whose feature chain is updated.
     * @param p_fallback_feature Plugin-owned fallback structure retained through device creation.
     * @param p_feature Boolean feature member to enable.
     * @throws std::runtime_error If the arguments are invalid or the feature is unsupported.
     */
    template <VkStructureType StructureType, typename FeatureType>
    static void enable_device_feature(VkPhysicalDevice p_physical_device,
        VkDeviceCreateInfo* p_create_info,
        FeatureType* p_fallback_feature,
        VkBool32 FeatureType::* p_feature) {
        if (p_physical_device == VK_NULL_HANDLE || p_create_info == nullptr || p_fallback_feature == nullptr || p_feature == nullptr) {
            throw std::runtime_error("Failed to enable Vulkan device feature: invalid arguments.");
        }
        if (!check_device_feature_support<StructureType>(p_physical_device, p_feature)) {
            throw std::runtime_error("Failed to enable Vulkan device feature: requested feature is not supported.");
        }
        auto* feature = reinterpret_cast<VkBaseOutStructure*>(const_cast<void*>(p_create_info->pNext));
        VkBaseOutStructure* tail = nullptr;
        while (feature != nullptr) {
            if (feature->sType == StructureType) {
                reinterpret_cast<FeatureType*>(feature)->*p_feature = VK_TRUE;
                return;
            }
            tail = feature;
            feature = feature->pNext;
        }
        p_fallback_feature->sType = StructureType;
        p_fallback_feature->pNext = nullptr;
        p_fallback_feature->*p_feature = VK_TRUE;
        if (tail == nullptr) {
            p_create_info->pNext = p_fallback_feature;
        } else {
            tail->pNext = reinterpret_cast<VkBaseOutStructure*>(p_fallback_feature);
        }
    }

    /**
     * @brief Check that every requested extension is exposed globally or by an enabled layer.
     */
    template <typename EnumerateFunction>
    static bool check_extension_support(std::span<const char* const> p_extensions,
        std::span<const char* const> p_enabled_layers,
        const EnumerateFunction& p_enumerate_extensions) {
        std::set<std::string> unsupported_extensions(p_extensions.begin(), p_extensions.end());
        auto remove_supported_extensions = [&](const char* p_layer_name) {
            VkResult result = VK_SUCCESS;
            do {
                uint32_t extension_count = 0;
                result = p_enumerate_extensions(p_layer_name, &extension_count, nullptr);
                if (result != VK_SUCCESS) {
                    return false;
                }
                std::vector<VkExtensionProperties> extension_properties(extension_count);
                result = p_enumerate_extensions(p_layer_name, &extension_count, extension_properties.data());
                if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
                    return false;
                }
                if (result == VK_SUCCESS) {
                    for (const auto& extension : extension_properties) {
                        unsupported_extensions.erase(extension.extensionName);
                    }
                }
            } while (result == VK_INCOMPLETE);
            return true;
        };
        if (!remove_supported_extensions(nullptr)) {
            return false;
        }
        for (const char* layer_name : p_enabled_layers) {
            if (!remove_supported_extensions(layer_name)) {
                return false;
            }
        }
        return unsupported_extensions.empty();
    }
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
