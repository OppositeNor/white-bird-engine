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
#include "function/render_vk/renderer_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/engine_core.hh"
#include "core/job/job_buffer_ring_mpsc.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/engine_function.hh"
#include "function/function_utils.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_graph_resource_binding.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_plugin_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/render_vk_resource_unit_loader.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_staging_buffer_vk.hh"
#include "function/render_vk/vma_vk.hh"
#include "function/window/i_window.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

template <VkStructureType StructureType, typename FeatureType>
static bool check_device_feature_support(VkPhysicalDevice p_device, VkBool32 FeatureType::* p_feature) {
    FeatureType feature{};
    feature.sType = StructureType;

    VkPhysicalDeviceFeatures2 device_features{};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features.pNext = &feature;
    vkGetPhysicalDeviceFeatures2(p_device, &device_features);

    return feature.*p_feature == VK_TRUE;
}

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

RendererVK::RendererVK(AllocType* p_allocator, Ref<IWindow> p_window, Ref<JobBufferRingMPSC> p_job_buffer, RendererCapabilities& p_capabilities)
    : job_buffer(p_job_buffer), allocator(p_allocator), render_database(make_ref<RenderDatabase>(p_allocator, p_allocator)),
      window(p_window), in_flight_fences(p_allocator), image_available_semaphores(p_allocator), render_graphs(p_allocator),
      base_render_data(p_allocator), registered_plugins(p_allocator) {
    WBE_DEBUG_ASSERT(p_job_buffer != MEM_NULL);
    WBE_DEBUG_ASSERT(p_window != MEM_NULL);
    WBE_DEBUG_ASSERT(p_allocator != nullptr);
    singleton = this;
    for (auto& delayed_call : delayed_calls) {
        delayed_call.second = -1;
    }
    register_renderer_vk_resource_unit_loaders();
    init(p_capabilities);
}

RendererVK::~RendererVK() {
    terminate();
    singleton = nullptr;
}

void RendererVK::tick(double p_delta) {
    WBE_DEBUG_ASSERT(!(this_ref.is_null()));

    execute_graph(active_graph, p_delta);
}

void RendererVK::execute_graph(Ref<RenderGraphVK> p_graph, double p_delta) {
    // Setup pipeline.
    auto tick_frame_counter = tick_setup_pipeline();
    if (!tick_frame_counter.has_value()) {
        // No pipeline selected, continue.
        return;
    }

    // Frame-local resource updates must run after the slot is selected and its fence has completed.
    run_stored_jobs();

    run_delayed_calls();
    run_gpu_jobs();
    uint32_t frame_counter = *tick_frame_counter;

    if (p_graph == MEM_NULL) {
        // No active render graph, nothing to do this frame.
        return;
    }

    auto& image_available_semaphore = image_available_semaphores[frame_counter];
    auto next_image_index = acquire_swapchain_next_image(image_available_semaphore->get_semaphore());
    if (!next_image_index.has_value()) {
        // Swap chain resetted, needs re-acquire. Skip this frame's rendering.
        return;
    }
    uint32_t image_index = *next_image_index;

    vkResetFences(logical_device, 1, &(base_render_data.in_flight_fence));

    // Inject the current swap-chain image view into the active graph so tasks
    // referencing REND_RES_SWAP_CHAIN_IMAGE resolve to the correct
    // per-frame target.
    base_render_data.current_swap_chain_index = image_index;
    p_graph->set_resource(
        REND_RES_SWAP_CHAIN_IMAGE,
        make_ref<ROCSingleVK>(allocator, render_objects->swap_chain_image_views[image_index]));
    set_graph_static_opaque_resources(p_graph);

    // Tell the graph to wait on the swap-chain image-available binary semaphore
    // at COLOR_ATTACHMENT_OUTPUT.
    VkSemaphoreSubmitInfo wait_info = image_available_semaphore->get_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    std::array<VkSemaphoreSubmitInfo, 1> wait_infos{wait_info};
    std::span<const VkSemaphoreSubmitInfo> extra_signals{};
    p_graph->execute(
        base_render_data.current_frame + 1, p_delta, std::span<const VkSemaphoreSubmitInfo>(wait_infos.data(), wait_infos.size()), extra_signals);

    present(image_index, p_graph);
    p_graph->reset_transient_resources();

    ++(base_render_data.current_frame);
}

template <typename T>
static void update_buffer(const BaseRenderDataVK* p_render_data,
    DeviceDomain p_domain,
    Ref<ROBufferVK>& p_buffer,
    const Vector<T, RenderAllocator>& p_from_buffer,
    VkBufferUsageFlags p_buffer_usage,
    Ref<ROStagingBufferVK> p_staging_buffer) {
    size_t size_in_bytes = p_from_buffer.size() * sizeof(T);
    if (p_buffer == MEM_NULL || size_in_bytes > p_buffer->fields().buffer_size) {
        // If res not initialized, or size exceeds buffer capacity, create a new
        // buffer.
        ROBufferVK::Builder builder;
        builder.set_domain(p_domain).set_usage(p_buffer_usage).set_size(size_in_bytes);
        p_buffer = builder.create_ref(p_render_data);
    }
    switch (p_domain) {
    case DeviceDomain::HOST_DEVICE:
    case DeviceDomain::HOST:
        p_buffer->update_buffer(p_from_buffer.data(), size_in_bytes);
        break;
    case DeviceDomain::DEVICE: {
        // Setup a temporary host buffer for copying the data.
        ROBufferVK::Builder temp_buffer_builder;
        temp_buffer_builder.set_domain(DeviceDomain::HOST).add_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT).set_size(size_in_bytes);
        void* data = p_staging_buffer->allocate(size_in_bytes);
        memcpy(data, p_from_buffer.data(), size_in_bytes);
        p_buffer->update_buffer(p_staging_buffer, data, size_in_bytes);
    } break;
    }
}

void RendererVK::init(RendererCapabilities& p_capabilities) {
    stdout_log(WBE_CHANNEL_DEBUG)->message("Initializing Vulkan renderer.");
    render_objects = make_unique<RenderObjects>(allocator, allocator);
    register_requested_plugins(p_capabilities);
    init_vulkan(p_capabilities);
    base_render_data.renderer_capabilities = p_capabilities;
    setup_base_render_data();
    // current_frame is a monotonic counter that drives the per-frame timeline
    // semaphore values. It must be initialized exactly once here, NOT in
    // setup_base_render_data() (which is also re-invoked on swap chain
    // recreation) — otherwise the timeline value would regress and
    // re-signal a value already reached, breaking VUID-VkSubmitInfo2-semaphore-03882.
    base_render_data.current_frame = 1;
    init_swap_chain_image_views();
    image_available_semaphores.reserve(frames_in_flight);
    for (size_t i = 0; i < frames_in_flight; ++i) {
        image_available_semaphores.emplace_back(make_ref<SemaphoreVK>(allocator, &base_render_data, true));
    }
    stdout_log(WBE_CHANNEL_DEBUG)->message("Vulkan renderer initialized.");
}

void RendererVK::terminate() {
    vkDeviceWaitIdle(logical_device);
    registered_plugins.clear();
    render_objects.reset();
    base_render_data.static_opaque_acceleration_structure = VK_NULL_HANDLE;

    // Release the active render graph and any registered render graph descriptors.
    // These hold Refs to render-object resources (buffers, images, etc.) whose
    // destructors enqueue device-object cleanup via register_delayed_call.
    // We must drop them before draining the delayed-call list, otherwise the
    // resources outlive vkDestroyDevice() and trigger validation leaks.
    active_graph = MEM_NULL;
    render_graphs.clear();
    render_database->clear_compiled_render_resources();
    image_available_semaphores.clear();

    synchronize_gpu_and_flush_delayed_calls();
    for (auto* fence : in_flight_fences) {
        vkDestroyFence(logical_device, fence, get_allocation_callbacks());
    }
    vkDestroyDescriptorPool(logical_device, descriptor_pool, get_allocation_callbacks());
    vkDestroyCommandPool(logical_device, compute_command_pool, get_allocation_callbacks());
    vkDestroyCommandPool(logical_device, graphics_command_pool, get_allocation_callbacks());
    vkDestroySwapchainKHR(logical_device, swap_chain, get_allocation_callbacks());
    vkDestroySurfaceKHR(vk_instance, surface, nullptr);
    vmaDestroyAllocator(vma_allocator);
    vma_allocator = VK_NULL_HANDLE;
    vkDestroyDevice(logical_device, get_allocation_callbacks());
    vkDestroyInstance(vk_instance, get_allocation_callbacks());
}

void RendererVK::register_requested_plugins(const RendererCapabilities& p_capabilities) {
    register_builtin_render_plugins_vk(registered_plugins, allocator, &base_render_data, p_capabilities);
}

void RendererVK::init_vulkan(RendererCapabilities& p_capabilities) {
    create_instance();
    create_device(p_capabilities);
    create_vma_allocator();
    create_swap_chain();
    create_command_pools();
    create_gpu_job_command_buffer();
    create_descriptor_pool();
    create_async_objects();
}

void RendererVK::recreate_swap_chain() {
    vkDeviceWaitIdle(logical_device);
    clean_up_swap_chain();
    create_swap_chain();
    init_swap_chain_image_views();
    setup_base_render_data();
}

void RendererVK::setup_base_render_data() {
    base_render_data.allocator = allocator;
    base_render_data.render_database = render_database.get();
    base_render_data.logic_device = logical_device;
    base_render_data.physical_device = physical_device;
    base_render_data.vma_allocator = vma_allocator;
    base_render_data.swap_chain_format = swap_chain_image_format;
    base_render_data.swap_chain_extent = swap_chain_extent;
    base_render_data.swap_chain_aspect_ratio =
        static_cast<float>(swap_chain_extent.width) / static_cast<float>(swap_chain_extent.height);
    base_render_data.allocation_callbacks = get_allocation_callbacks();
    base_render_data.graphics_queue = graphics_queue;
    base_render_data.compute_queue = compute_queue;
    base_render_data.viewport = viewport;
    base_render_data.scissor = scissor;
    base_render_data.graphics_command_pool = graphics_command_pool;
    base_render_data.compute_command_pool = compute_command_pool;
    base_render_data.descriptor_pool = descriptor_pool;
    VkPhysicalDeviceProperties2 physical_device_properties{};
    physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(physical_device, &physical_device_properties);
    base_render_data.max_variable_sampled_image_descriptor_count = std::min({static_cast<uint32_t>(MAX_SAMPLED_IMAGE_DESCRIPTOR_COUNT),
        physical_device_properties.properties.limits.maxPerStageDescriptorSampledImages,
        physical_device_properties.properties.limits.maxDescriptorSetSampledImages,
        physical_device_properties.properties.limits.maxPerStageResources});
    base_render_data.max_draw_indirect_count = physical_device_properties.properties.limits.maxDrawIndirectCount;
    for (auto& plugin : registered_plugins) {
        plugin->update_base_render_data(&base_render_data, physical_device);
    }
    if (!in_flight_fences.empty()) {
        base_render_data.in_flight_fence = in_flight_fences[0];
    } else {
        base_render_data.in_flight_fence = VK_NULL_HANDLE;
    }
    base_render_data.current_swap_chain_index = 0;
    base_render_data.swap_chain_images_count = swap_chain_images_count;
    base_render_data.queue_family_indices = queue_family_indices.get_unique_indices();
    base_render_data.register_delayed_call = [this](std::function<void()>&& p_function, uint32_t p_delayed_frames) {
        register_delay_call(std::move(p_function), p_delayed_frames);
    };
    base_render_data.frames_in_flight = frames_in_flight;
}

void RendererVK::create_instance() {
    // Get configuration informations.
    const auto& engine_config = EngineCore::get_singleton()->engine_config->get_config_options();
    const auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();

    // Setup IApplication info.
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = game_config.game_name.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(game_config.version_major, game_config.version_minor, game_config.version_patch);
    app_info.pEngineName = engine_config.engine_name;
    app_info.engineVersion = VK_MAKE_VERSION(engine_config.VERSION_MAJOR, engine_config.VERSION_MINOR, engine_config.VERSION_PATCH);
    app_info.apiVersion = VULKAN_API_VERSION;

    // Setup instance create info.
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    check_layer_support();
    create_info.enabledLayerCount = enabled_layers.size();
    create_info.ppEnabledLayerNames = enabled_layers.data();

    // Setup and validate instance extensions.
    std::vector<const char*> required_extensions = window->get_required_vulkan_instance_extensions();
    size_t plugin_instance_extension_count = 0;
    for (const auto& plugin : registered_plugins) {
        plugin_instance_extension_count += plugin->get_required_instance_extensions().size();
    }
    required_extensions.reserve(required_extensions.size() + enabled_instance_extensions.size() + plugin_instance_extension_count);
    required_extensions.insert(required_extensions.end(), enabled_instance_extensions.begin(), enabled_instance_extensions.end());
    for (const auto& plugin : registered_plugins) {
        for (const char* required_extension : plugin->get_required_instance_extensions()) {
            required_extensions.push_back(required_extension);
        }
    }
    if (!check_extension_support(required_extensions,
            enabled_layers,
            [](const char* p_layer_name, uint32_t* p_extension_count, VkExtensionProperties* p_extension_properties) {
                return vkEnumerateInstanceExtensionProperties(p_layer_name, p_extension_count, p_extension_properties);
            })) {
        throw std::runtime_error("Required Vulkan instance extensions are not supported.");
    }

    create_info.enabledExtensionCount = required_extensions.size();
    create_info.ppEnabledExtensionNames = required_extensions.data();

    // Create instance.
    if (vkCreateInstance(&create_info, get_allocation_callbacks(), &vk_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance.");
    }
    window->init_vulkan(vk_instance);
    surface = window->get_vulkan_surface();
}

void RendererVK::create_device(RendererCapabilities& p_capabilities) {
    create_physical_device();
    for (size_t i = 0; i < registered_plugins.size();) {
        auto& plugin = registered_plugins[i];
        if (!plugin->try_enable(p_capabilities, physical_device, enabled_layers.data(), enabled_layers.size())) {
            registered_plugins.erase(registered_plugins.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        for (const char* required_extension : plugin->get_required_device_extensions()) {
            enabled_device_extensions.push_back(required_extension);
        }
        ++i;
    }
    create_logical_device();
}

void RendererVK::create_vma_allocator() {
    VmaAllocatorCreateFlags allocator_flags = 0;
    for (auto& plugin : registered_plugins) {
        if ((plugin->get_static_opaque_vertex_buffer_usage() & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0 ||
            (plugin->get_static_opaque_index_buffer_usage() & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0) {
            allocator_flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }
    }
    VmaAllocatorCreateInfo create_info{
        .flags = allocator_flags,
        .physicalDevice = physical_device,
        .device = logical_device,
        .pAllocationCallbacks = get_allocation_callbacks(),
        .instance = vk_instance,
        .vulkanApiVersion = VULKAN_API_VERSION,
    };
    VkResult result = vmaCreateAllocator(&create_info, &vma_allocator);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create Vulkan memory allocator: {}.", string_VkResult(result)));
    }
}

void RendererVK::create_swap_chain() {
    // Get informations to create swap chain.
    SwapChainSupportDetails details = get_swap_chain_support_details(physical_device);
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(details.formats);
    VkPresentModeKHR present_mode = choose_swap_present_mode(details.present_modes);
    VkExtent2D extent = choose_swap_extent(details.capabilities);
    uint32_t image_count = 2;
    if (details.capabilities.maxImageCount != 0) {
        image_count = std::clamp(image_count, details.capabilities.minImageCount, details.capabilities.maxImageCount);
    } else {
        image_count = std::max(image_count, details.capabilities.minImageCount);
    }

    // Setup swap chain create info.
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    auto unique_indices = queue_family_indices.get_unique_indices();
    Vector<uint32_t> swap_chain_queue_family_indices_array(allocator);

    // If we have to use the image between multiple queue families, we have to
    // specify it.
    if (unique_indices.size() > 1) {
        // Images can share ownership. If other queue families want to access we
        // don't have to explicitly transfer ownership.
        swap_chain_queue_family_indices_array = Vector<uint32_t>(unique_indices.begin(), unique_indices.end(), allocator);
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = unique_indices.size();
        create_info.pQueueFamilyIndices = swap_chain_queue_family_indices_array.data();
    } else {
        // Images cannot share ownership. If other queue families want to access
        // we have to explicitly transfer ownership.
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    create_info.preTransform = details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Ignore alpha channel
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(logical_device, &create_info, get_allocation_callbacks(), &swap_chain);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain.");
    }

    // Create swap chain images.
    vkGetSwapchainImagesKHR(logical_device, swap_chain, &swap_chain_images_count, nullptr);
    Vector<VkImage> swap_chain_images(allocator);
    swap_chain_images.resize(swap_chain_images_count);
    vkGetSwapchainImagesKHR(logical_device, swap_chain, &swap_chain_images_count, swap_chain_images.data());
    WBE_DEBUG_ASSERT(render_objects->swap_chain_images.empty());
    render_objects->swap_chain_images.reserve(swap_chain_images_count);
    for (auto& swap_chain_image : swap_chain_images) {
        render_objects->swap_chain_images.emplace_back(
            make_ref<ROImageVK>(allocator, &base_render_data, swap_chain_image, VK_IMAGE_LAYOUT_UNDEFINED));
    }
    const auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    frames_in_flight = game_config.frames_in_flight;
    if (frames_in_flight < swap_chain_images_count) {
        stdout_log(WBE_CHANNEL_RENDER)
            ->warning(std::format("frames_in_flight is required to be at least number of swap "
                                  "chain images; but get {} frames_in_flight and {} images. "
                                  "Resizing frames_in_flight to {}.",
                frames_in_flight,
                swap_chain_images_count,
                swap_chain_images_count));
        frames_in_flight = swap_chain_images_count;
    }
    swap_chain_image_format = surface_format.format;
    swap_chain_extent = extent;

    // Setup viewport and scissor information.
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = extent;
}

void RendererVK::clean_up_swap_chain() {
    render_objects->swap_chain_image_views.clear();
    render_objects->swap_chain_images.clear();
    vkDestroySwapchainKHR(logical_device, swap_chain, get_allocation_callbacks());
}

void RendererVK::init_swap_chain_image_views() {
    render_objects->swap_chain_image_views.reserve(render_objects->swap_chain_images.size());
    for (auto& swap_chain_image : render_objects->swap_chain_images) {
        VkImageSubresourceRange subresource_range;
        // Only use the color information of the image.
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Map to a mipmap level, map to the original image, which is 0.
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        ROImageViewVK::Builder builder;
        builder.set_image(swap_chain_image)
            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
            .set_format(swap_chain_image_format)
            .set_components(VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY)
            .set_subresource_range(subresource_range);

        render_objects->swap_chain_image_views.emplace_back(builder.create_ref(&base_render_data));
    }
}

void RendererVK::create_command_pools() {
    VkCommandPoolCreateInfo graphics_pool_create_info{};
    graphics_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    graphics_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    graphics_pool_create_info.queueFamilyIndex = queue_family_indices.get_graphics();
    if (vkCreateCommandPool(logical_device, &graphics_pool_create_info, get_allocation_callbacks(), &graphics_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool.");
    }
    VkCommandPoolCreateInfo compute_pool_create_info{};
    compute_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    compute_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    compute_pool_create_info.queueFamilyIndex = queue_family_indices.get_compute();
    if (vkCreateCommandPool(logical_device, &compute_pool_create_info, get_allocation_callbacks(), &compute_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute command pool.");
    }
}

void RendererVK::create_gpu_job_command_buffer() {
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = graphics_command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(logical_device, &allocate_info, &gpu_job_command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate GPU-job command buffer.");
    }
}

void RendererVK::create_descriptor_pool() {
    uint64_t descriptor_generation_capacity = static_cast<uint64_t>(frames_in_flight) * 2 + 2;
    auto scale_descriptor_capacity = [descriptor_generation_capacity](size_t p_capacity) -> uint32_t {
        uint64_t scaled_capacity = static_cast<uint64_t>(p_capacity) * descriptor_generation_capacity;
        if (scaled_capacity > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to create descriptor pool: generation capacity exceeds Vulkan limits.");
        }
        return static_cast<uint32_t>(scaled_capacity);
    };
    std::array<VkDescriptorPoolSize, 5> pool_sizes{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = scale_descriptor_capacity(MAX_UNIFORM_DESCRIPTOR_COUNT)},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = scale_descriptor_capacity(MAX_SAMPLED_IMAGE_DESCRIPTOR_COUNT)},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = scale_descriptor_capacity(MAX_STORAGE_IMAGE_DESCRIPTOR_COUNT)},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = scale_descriptor_capacity(MAX_SAMPLER_DESCRIPTOR_COUNT)},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = scale_descriptor_capacity(MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT)},
    };

    VkDescriptorPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    create_info.maxSets = scale_descriptor_capacity(MAX_DESCRIPTOR_SET_COUNT);
    create_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    create_info.pPoolSizes = pool_sizes.data();
    if (vkCreateDescriptorPool(logical_device, &create_info, get_allocation_callbacks(), &descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool.");
    }
}

void RendererVK::create_async_objects() {
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Set the initial state of the fence to "signaled", since we don't have a
    // "previous frame" at the first frame.
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    in_flight_fences.reserve(frames_in_flight);
    for (size_t i = 0; i < frames_in_flight; ++i) {
        VkFence fence = VK_NULL_HANDLE;
        if (vkCreateFence(logical_device, &fence_create_info, get_allocation_callbacks(), &fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create in flight fence");
        }
        in_flight_fences.push_back(fence);
    }
}

void RendererVK::create_physical_device() {
    // Enumerate all physical devices.
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, nullptr);
    if (physical_device_count == 0) {
        throw std::runtime_error("No valid physical device found.");
    }
    Vector<VkPhysicalDevice> physical_devices(physical_device_count, allocator);
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, physical_devices.data());

    // Pick one that's the most suitable.
    pick_physical_device(physical_devices);
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    String device_name(device_properties.deviceName, allocator);
    stdout_log(WBE_CHANNEL_DEBUG)->message(std::format("Picked video device: \"{}\".", device_name));
    queue_family_indices = get_queue_family_indices(physical_device);
}

void RendererVK::create_logical_device() {
    // Setup queues create info.
    float queue_priority = 1.0;
    Set<uint32_t> indices = queue_family_indices.get_unique_indices();
    Vector<VkDeviceQueueCreateInfo> queues_create_info(allocator);
    queues_create_info.reserve(indices.size());
    for (uint32_t elem : indices) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = elem;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queues_create_info.push_back(queue_create_info);
    }

    // Setup logical device create info.
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = indices.size();
    create_info.pQueueCreateInfos = queues_create_info.data();

    // Setup layers.
    // Device Layers have never worked since Vulkan 1.0 and only Instance Layers should be used instead:
    // https://docs.vulkan.org/spec/latest/appendices/legacy.html#legacy-devicelayers.
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;

    // Setup logical device extensions.
    create_info.enabledExtensionCount = enabled_device_extensions.size();
    create_info.ppEnabledExtensionNames = enabled_device_extensions.data();
    // Setup physical device features.
    VkPhysicalDeviceFeatures device_features{};
    device_features.depthClamp = VK_TRUE;
    device_features.multiDrawIndirect = VK_TRUE;
    device_features.drawIndirectFirstInstance = VK_TRUE;
    device_features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    device_features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
    create_info.pEnabledFeatures = &device_features;

    VkPhysicalDeviceTimelineSemaphoreFeatures semaphore_features{};
    semaphore_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    semaphore_features.timelineSemaphore = VK_TRUE;
    create_info.pNext = &semaphore_features;

    VkPhysicalDeviceSynchronization2Features sync2_features{};
    sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2_features.synchronization2 = VK_TRUE;
    semaphore_features.pNext = &sync2_features;

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{};
    dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering_features.dynamicRendering = VK_TRUE;
    sync2_features.pNext = &dynamic_rendering_features;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
    descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptor_indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    descriptor_indexing_features.runtimeDescriptorArray = VK_TRUE;
    dynamic_rendering_features.pNext = &descriptor_indexing_features;

    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic_state_features{};
    vertex_input_dynamic_state_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
    vertex_input_dynamic_state_features.vertexInputDynamicState = VK_TRUE;
    descriptor_indexing_features.pNext = &vertex_input_dynamic_state_features;
    VkBaseOutStructure* feature_chain_tail = reinterpret_cast<VkBaseOutStructure*>(&vertex_input_dynamic_state_features);
    for (auto& plugin : registered_plugins) {
        VkBaseOutStructure* plugin_feature_chain = plugin->get_device_feature_chain();
        if (plugin_feature_chain == nullptr) {
            continue;
        }
        feature_chain_tail->pNext = plugin_feature_chain;
        while (feature_chain_tail->pNext != nullptr) {
            feature_chain_tail = feature_chain_tail->pNext;
        }
    }

    // Create logical device.
    if (vkCreateDevice(physical_device, &create_info, get_allocation_callbacks(), &logical_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }
    base_render_data.command_set_vertex_input =
        reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(vkGetDeviceProcAddr(logical_device, "vkCmdSetVertexInputEXT"));
    if (base_render_data.command_set_vertex_input == nullptr) {
        throw std::runtime_error("Failed to load vkCmdSetVertexInputEXT.");
    }
    for (auto& plugin : registered_plugins) {
        plugin->on_device_created(&base_render_data, physical_device, logical_device);
    }

    // Get the device queues.
    vkGetDeviceQueue(logical_device, queue_family_indices.get_graphics(), 0, &graphics_queue);
    vkGetDeviceQueue(logical_device, queue_family_indices.get_compute(), 0, &compute_queue);
    vkGetDeviceQueue(logical_device, queue_family_indices.get_present(), 0, &present_queue);
}

bool RendererVK::check_layer_support() {
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    Vector<VkLayerProperties> layer_properties(layer_count, allocator);
    vkEnumerateInstanceLayerProperties(&layer_count, layer_properties.data());
    for (const auto* name : enabled_layers) {
        bool layer_found = false;
        for (auto& layer_prop : layer_properties) {
            if (strcmp(layer_prop.layerName, name) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }
    return true;
}

void RendererVK::pick_physical_device(const Vector<VkPhysicalDevice>& p_physical_devices) {
    physical_device = VK_NULL_HANDLE;
    const auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    if (game_config.render_device_index.has_value()) {
        uint32_t pick_device_index = game_config.render_device_index.value();
        // The refered device index is provided, use the index to pick device.
        // If failed to pick, fall back to automatic picking.
        if (std::cmp_greater_equal(pick_device_index, p_physical_devices.size())) {
            // Index out of range.
            stdout_log(WBE_CHANNEL_DEBUG)
                ->warning(std::format("Failed to use the device provided by the configuration: "
                                      "Index {} out of range (total devices: {}). Picking"
                                      " an alternative device.",
                    pick_device_index,
                    p_physical_devices.size()));
        } else if (!is_device_suitable(p_physical_devices[pick_device_index])) {
            // Device is not suitable.
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(p_physical_devices[pick_device_index], &device_properties);
            String device_name(device_properties.deviceName, allocator);
            stdout_log(WBE_CHANNEL_DEBUG)
                ->warning(std::format("Failed to use the device provided by the "
                                      "configuration: device \"{}\" is not suitable."
                                      " Picking an alternative device.",
                    device_name));
        } else {
            // Successfully pick the device.
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(p_physical_devices[pick_device_index], &device_properties);
            String device_name(device_properties.deviceName, allocator);
            physical_device = p_physical_devices[pick_device_index];
            stdout_log(WBE_CHANNEL_DEBUG)->message(std::format("Picked device: \"{}\".", device_name));
            return;
        }
    }
    // Pick a suitable device.
    for (const auto& device : p_physical_devices) {
        if (!is_device_suitable(device)) {
            continue;
        }
        physical_device = device;
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            break;
        }
    }
    if (physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("No valid physical device found.");
    }
}

bool RendererVK::is_device_suitable(VkPhysicalDevice p_device) {
    VkPhysicalDeviceProperties device_properties{};
    vkGetPhysicalDeviceProperties(p_device, &device_properties);
    if (device_properties.apiVersion < VULKAN_API_VERSION) {
        return false;
    }
    QueueFamilyIndices device_indices = get_queue_family_indices(p_device);
    if (!device_indices.is_complete()) {
        return false;
    }
    if (!check_extension_support(enabled_device_extensions,
            enabled_layers,
            [p_device](const char* p_layer_name, uint32_t* p_extension_count, VkExtensionProperties* p_extension_properties) {
                return vkEnumerateDeviceExtensionProperties(p_device, p_layer_name, p_extension_count, p_extension_properties);
            })) {
        return false;
    }
    SwapChainSupportDetails swap_chain_support_details = get_swap_chain_support_details(p_device);
    if (swap_chain_support_details.formats.empty() || swap_chain_support_details.present_modes.empty()) {
        return false;
    }
    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
    descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic_state_features{};
    vertex_input_dynamic_state_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
    descriptor_indexing_features.pNext = &vertex_input_dynamic_state_features;
    VkPhysicalDeviceFeatures2 device_features{};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features.pNext = &descriptor_indexing_features;
    vkGetPhysicalDeviceFeatures2(p_device, &device_features);
    if (device_features.features.depthClamp != VK_TRUE || device_features.features.multiDrawIndirect != VK_TRUE ||
        device_features.features.drawIndirectFirstInstance != VK_TRUE ||
        device_features.features.shaderSampledImageArrayDynamicIndexing != VK_TRUE ||
        device_features.features.shaderUniformBufferArrayDynamicIndexing != VK_TRUE ||
        descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing != VK_TRUE ||
        descriptor_indexing_features.descriptorBindingVariableDescriptorCount != VK_TRUE ||
        descriptor_indexing_features.runtimeDescriptorArray != VK_TRUE ||
        vertex_input_dynamic_state_features.vertexInputDynamicState != VK_TRUE) {
        return false;
    }

    // End of checks, the device is suitable.
    return true;
}

RendererVK::QueueFamilyIndices RendererVK::get_queue_family_indices(VkPhysicalDevice p_device) {
    QueueFamilyIndices indices{};
    indices.allocator = allocator;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_family_count, nullptr);
    Vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count, allocator);
    vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_family_count, queue_family_properties.data());

    // Gather present support for every queue family up front.
    Vector<VkBool32> present_support(queue_family_count, allocator);
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        present_support[i] = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface, &present_support[i]);
    }

    // Pass 1: prefer dedicated queues.
    //   Compute: COMPUTE without GRAPHICS  →  dedicated async-compute.
    //   Graphics: GRAPHICS without COMPUTE  →  dedicated rasterisation.
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        const VkQueueFlags flags = queue_family_properties[i].queueFlags;
        const bool has_graphics = (flags & VK_QUEUE_GRAPHICS_BIT) != 0;
        const bool has_compute = (flags & VK_QUEUE_COMPUTE_BIT) != 0;
        if (has_compute && !has_graphics && !indices.compute.has_value()) {
            indices.compute = i;
        }
        if (has_graphics && !has_compute && !indices.graphics.has_value()) {
            indices.graphics = i;
        }
    }

    // Pass 2: fill in still-missing queues, preferring indices not already taken.
    //   Present prefers the graphics queue family to avoid concurrent swapchain sharing.
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        const VkQueueFlags flags = queue_family_properties[i].queueFlags;
        if (!indices.graphics.has_value() && (flags & VK_QUEUE_GRAPHICS_BIT)) {
            if (!indices.compute.has_value() || i != *indices.compute) {
                indices.graphics = i;
            }
        }
        if (!indices.compute.has_value() && (flags & VK_QUEUE_COMPUTE_BIT)) {
            if (!indices.graphics.has_value() || i != *indices.graphics) {
                indices.compute = i;
            }
        }
        if (!indices.present.has_value() && present_support[i]) {
            if (indices.graphics.has_value() && i == *indices.graphics) {
                indices.present = i;
            }
        }
    }

    // Pass 3: last-resort fallback — accept any suitable queue with no uniqueness constraint.
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        const VkQueueFlags flags = queue_family_properties[i].queueFlags;
        if (!indices.graphics.has_value() && (flags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.graphics = i;
        }
        if (!indices.compute.has_value() && (flags & VK_QUEUE_COMPUTE_BIT)) {
            indices.compute = i;
        }
        if (!indices.present.has_value() && present_support[i]) {
            indices.present = i;
        }
    }

    return indices;
}

RendererVK::SwapChainSupportDetails RendererVK::get_swap_chain_support_details(VkPhysicalDevice p_device) {
    SwapChainSupportDetails details{
        .formats = Vector<VkSurfaceFormatKHR>(allocator), .present_modes = Vector<VkPresentModeKHR>(allocator)};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_device, surface, &details.capabilities);
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &format_count, nullptr);
    details.formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &format_count, details.formats.data());
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &present_mode_count, nullptr);
    details.present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &present_mode_count, details.present_modes.data());
    return details;
}

VkSurfaceFormatKHR RendererVK::choose_swap_surface_format(const Vector<VkSurfaceFormatKHR>& p_available_formats) {
    for (const auto& available_format : p_available_formats) {
        if (available_format.format == VK_FORMAT_R8G8B8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }
    return p_available_formats[0];
}

VkPresentModeKHR RendererVK::choose_swap_present_mode(const Vector<VkPresentModeKHR>& p_available_present_modes) {
    for (const auto& available_present_mode : p_available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D RendererVK::choose_swap_extent(const VkSurfaceCapabilitiesKHR& p_capabilities) {
    if (p_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return p_capabilities.currentExtent;
    }
    uint32_t extent_width = 0, extent_height = 0;
    window->get_size_in_pixels(extent_width, extent_height);
    VkExtent2D result{.width = extent_width, .height = extent_height};
    return result;
}

void RendererVK::setup_viewport_and_scissor_info(VkExtent2D p_swap_chain_extent) {
    // Setup viewport and scissor information.
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(p_swap_chain_extent.width);
    viewport.height = static_cast<float>(p_swap_chain_extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = p_swap_chain_extent;
}

void RendererVK::run_stored_jobs() {
    // Run some jobs stored in the job buffer.
    for (size_t i = 0; i < RUN_MAX_JOBS_PER_FRAME; ++i) {
        std::function<void()> job = job_buffer->retrieve_job();
        if (job == nullptr) {
            break;
        }
        job();
    }
}

std::optional<uint32_t> RendererVK::acquire_swapchain_next_image(VkSemaphore p_image_avialable_semaphore) {
    WBE_DEBUG_ASSERT(p_image_avialable_semaphore != VK_NULL_HANDLE);
    uint32_t image_index;
    auto* image_available_semaphore = p_image_avialable_semaphore;
    VkResult result =
        vkAcquireNextImageKHR(logical_device, swap_chain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Swapchian outdated, needs to recreate.
        recreate_swap_chain();
        return std::nullopt;
    }
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image.");
    }
    return image_index;
}

std::optional<uint32_t> RendererVK::tick_setup_pipeline() {
    static uint32_t frame_counter = 0;
    frame_counter = (frame_counter + 1) % frames_in_flight;
    base_render_data.in_flight_fence = in_flight_fences[frame_counter];
    base_render_data.current_frame_slot = frame_counter;

    vkWaitForFences(logical_device, 1, &(base_render_data.in_flight_fence), VK_TRUE, UINT64_MAX);
    return frame_counter;
}

void RendererVK::run_delayed_calls() {
    for (auto& delayed_call : delayed_calls) {
        if (delayed_call.second < 0) {
            continue;
        }
        --(delayed_call.second);
        if (delayed_call.second < 0) {
            // Run delayed calls that should be called in the current frame.
            delayed_call.first();
            delayed_call.first = nullptr;
        }
    }
}

void RendererVK::register_gpu_job(GPUJobVK p_job) {
    if (!p_job) {
        throw std::runtime_error("Failed to register GPU job: job is empty.");
    }
    for (auto& gpu_job : gpu_jobs) {
        if (gpu_job) {
            continue;
        }
        gpu_job = std::move(p_job);
        return;
    }
    throw std::runtime_error("Failed to register GPU job: GPU-job capacity is exhausted.");
}

void RendererVK::run_gpu_jobs() {
    bool has_job = false;
    for (const auto& gpu_job : gpu_jobs) {
        if (gpu_job) {
            has_job = true;
            break;
        }
    }
    if (!has_job) {
        return;
    }

    if (vkResetCommandBuffer(gpu_job_command_buffer, 0) != VK_SUCCESS) {
        throw std::runtime_error("Failed to reset GPU-job command buffer.");
    }
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(gpu_job_command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin GPU-job command buffer.");
    }
    Vector<GPUJobVK> running_jobs(allocator);
    for (auto& gpu_job : gpu_jobs) {
        if (!gpu_job) {
            continue;
        }
        running_jobs.push_back(std::move(gpu_job));
        gpu_job = nullptr;
        running_jobs.back()(gpu_job_command_buffer);
    }
    submit_gpu_job_command_buffer();
}

void RendererVK::submit_gpu_job_command_buffer() {
    VkMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    VkDependencyInfo dependency_info{};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.memoryBarrierCount = 1;
    dependency_info.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(gpu_job_command_buffer, &dependency_info);
    if (vkEndCommandBuffer(gpu_job_command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end GPU-job command buffer.");
    }

    VkCommandBufferSubmitInfo command_buffer_info{};
    command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_info.commandBuffer = gpu_job_command_buffer;
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_buffer_info;
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(logical_device, &fence_info, get_allocation_callbacks(), &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create GPU-job fence.");
    }
    if (vkQueueSubmit2(graphics_queue, 1, &submit_info, fence) != VK_SUCCESS) {
        vkDestroyFence(logical_device, fence, get_allocation_callbacks());
        throw std::runtime_error("Failed to submit GPU jobs.");
    }
    if (vkWaitForFences(logical_device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        vkDestroyFence(logical_device, fence, get_allocation_callbacks());
        throw std::runtime_error("Failed to submit GPU jobs.");
    }
    vkDestroyFence(logical_device, fence, get_allocation_callbacks());
}

void RendererVK::synchronize_gpu_and_flush_delayed_calls() {
    for (VkFence fence : in_flight_fences) {
        if (vkWaitForFences(logical_device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            throw std::runtime_error("Failed to synchronize GPU work before flushing delayed calls.");
        }
    }
    run_gpu_jobs();
    bool ran_any = true;
    while (ran_any) {
        ran_any = false;
        for (auto& delayed_call : delayed_calls) {
            if (!delayed_call.first) {
                continue;
            }
            auto call = std::move(delayed_call.first);
            delayed_call.first = nullptr;
            delayed_call.second = -1;
            call();
            ran_any = true;
        }
    }
}

void RendererVK::present(uint32_t p_image_index, Ref<RenderGraphVK> p_graph) {
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    VkSemaphore terminal = p_graph->get_terminal_semaphore()->get_semaphore();
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &terminal;

    VkSwapchainKHR swap_chains[] = {swap_chain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &p_image_index;

    present_info.pResults = nullptr;
    auto result = vkQueuePresentKHR(present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_swap_chain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image.");
    }
}

void RendererVK::register_render_graph(std::string_view p_resource_key, RendGraphDescVK p_desc) {
    if (render_graphs.contains(p_resource_key)) {
        throw std::runtime_error(std::format("Render graph with resource key: {} was already registered.", p_resource_key));
    }

    auto declare_runtime_resource = [&p_desc](std::string_view p_resource_id, RendResTID p_resource_type_id) {
        for (const auto& resource : p_desc.resources) {
            if (resource.res_id == p_resource_id) {
                return;
            }
        }
        p_desc.resources.push_back(RendResVK{.res_id = std::string(p_resource_id),
            .res_type_id = p_resource_type_id,
            .container = MEM_NULL});
    };
    declare_runtime_resource(REND_RES_SWAP_CHAIN_IMAGE, TypeUUID<ROImageViewVK>::UUID);
    declare_runtime_resource(REND_RES_STATIC_VERTICES, TypeUUID<ROBufferVK>::UUID);
    declare_runtime_resource(REND_RES_STATIC_INDICES, TypeUUID<ROBufferVK>::UUID);
    declare_runtime_resource(REND_RES_STATIC_OBJECT_IDS, TypeUUID<ROBufferVK>::UUID);

    auto desc_ref = make_ref<RendGraphDescVK>(allocator, std::move(p_desc));
    auto graph = make_ref<RenderGraphVK>(allocator, &base_render_data);
    graph->build_from_descriptor(*desc_ref);
    set_graph_static_opaque_resources(graph);

    render_graphs.emplace(std::string(p_resource_key),
        RegisteredRenderGraph{
            .descriptor = std::move(desc_ref),
            .graph = std::move(graph),
            .reference_count = 1,
        });
}

void RendererVK::register_render_graph(std::string_view p_resource_key) {
    auto graph_entry = render_graphs.find(p_resource_key);
    if (graph_entry == render_graphs.end()) {
        throw std::runtime_error(std::format("Render graph resource key was not loaded: {}.", p_resource_key));
    }
    ++graph_entry->second.reference_count;
}

void RendererVK::unregister_render_graph(std::string_view p_resource_key) {
    auto graph_entry = render_graphs.find(p_resource_key);
    if (graph_entry == render_graphs.end() || graph_entry->second.reference_count == 0) {
        throw std::runtime_error(std::format("Render graph with resource key: {} was not registered.", p_resource_key));
    }

    --graph_entry->second.reference_count;
    if (graph_entry->second.reference_count > 0) {
        return;
    }

    Ref<RenderGraphVK> released_graph = graph_entry->second.graph;
    Ref<RendGraphDescVK> released_descriptor = graph_entry->second.descriptor;
    if (active_graph == released_graph) {
        active_graph = MEM_NULL;
    }
    render_graphs.erase(graph_entry);
    register_delay_call(
        [released_graph, released_descriptor]() mutable {
            released_graph = MEM_NULL;
            released_descriptor = MEM_NULL;
        },
        frames_in_flight);
}

void RendererVK::activate_render_graph(std::string_view p_resource_key) {
    auto graph_entry = render_graphs.find(p_resource_key);
    if (graph_entry == render_graphs.end() || graph_entry->second.reference_count == 0) {
        throw std::runtime_error(std::format("Render graph with resource key: {} was not registered.", p_resource_key));
    }

    active_graph = graph_entry->second.graph;
    set_graph_static_opaque_resources(active_graph);
}

void RendererVK::update_render_graph_resources(
    std::string_view p_resource_key, const RenderGraphResourceBinding* p_bindings, size_t p_binding_count) {
    auto graph_entry = render_graphs.find(p_resource_key);
    if (graph_entry == render_graphs.end() || graph_entry->second.reference_count == 0) {
        throw std::runtime_error(std::format("Render graph with resource key: {} was not registered.", p_resource_key));
    }
    if (p_bindings == nullptr && p_binding_count != 0) {
        throw std::runtime_error("Render graph resource bindings are null.");
    }

    for (size_t i = 0; i < p_binding_count; ++i) {
        Ref<RenderObjectVK> resource = p_bindings[i].resource.dynamic_cast_ref<RenderObjectVK>();
        if (resource == MEM_NULL) {
            throw std::runtime_error(std::format("Render graph resource is not compatible with Vulkan: {}.", p_bindings[i].resource_id));
        }
        graph_entry->second.graph->set_transient_resource(
            p_bindings[i].resource_id,
            make_ref<ROCSingleVK>(allocator, std::move(resource)));
    }
}

void RendererVK::set_graph_static_opaque_resources(Ref<RenderGraphVK> p_graph) {
    if (p_graph == MEM_NULL) {
        return;
    }
    // Re-inject the global static-opaque mesh buffers so render tasks
    // referencing the global resource IDs can resolve them without per-pipeline
    // registration.
    if (render_objects->static_opaque_vertices != MEM_NULL) {
        p_graph->set_resource(
            REND_RES_STATIC_VERTICES,
            make_ref<ROCSingleVK>(allocator, render_objects->static_opaque_vertices));
    }
    if (render_objects->static_opaque_indices != MEM_NULL) {
        p_graph->set_resource(
            REND_RES_STATIC_INDICES,
            make_ref<ROCSingleVK>(allocator, render_objects->static_opaque_indices));
    }
    if (render_objects->static_opaque_obj_ids != MEM_NULL) {
        p_graph->set_resource(
            REND_RES_STATIC_OBJECT_IDS,
            make_ref<ROCSingleVK>(allocator, render_objects->static_opaque_obj_ids));
    }
}

template <typename T, typename SrcAlloc>
static Ref<ROBufferVK> build_or_resize_host_visible_buffer(
    const BaseRenderDataVK* p_render_data, Ref<ROBufferVK> p_existing, const Vector<T, SrcAlloc>& p_data, VkBufferUsageFlags p_usage) {
    const size_t size_in_bytes = p_data.size() * sizeof(T);
    if (size_in_bytes == 0) {
        return p_existing;
    }
    Ref<ROBufferVK> buffer = p_existing;
    if (buffer == MEM_NULL || size_in_bytes > buffer->fields().buffer_size) {
        buffer = ROBufferVK::Builder().set_domain(DeviceDomain::HOST_DEVICE).set_size(size_in_bytes).set_usage(p_usage).create_ref(p_render_data);
    }
    buffer->update_buffer(p_data.data(), size_in_bytes);
    return buffer;
}

void RendererVK::update_static_opaque_triangles() {
    WBE_DEBUG_ASSERT(render_database != MEM_NULL);
    synchronize_gpu_and_flush_delayed_calls();
    render_database->refresh_static_opaque_triangles();
    const auto& info = render_database->get_static_opaque_triangles_info();

    VkBufferUsageFlags vertex_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkBufferUsageFlags index_usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    for (auto& plugin : registered_plugins) {
        vertex_usage |= plugin->get_static_opaque_vertex_buffer_usage();
        index_usage |= plugin->get_static_opaque_index_buffer_usage();
    }
    render_objects->static_opaque_vertices =
        build_or_resize_host_visible_buffer(&base_render_data, render_objects->static_opaque_vertices, info.vertices, vertex_usage);
    render_objects->static_opaque_indices =
        build_or_resize_host_visible_buffer(&base_render_data, render_objects->static_opaque_indices, info.indices, index_usage);
    render_objects->static_opaque_obj_ids = build_or_resize_host_visible_buffer(
        &base_render_data, render_objects->static_opaque_obj_ids, info.obj_id, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    for (auto& plugin : registered_plugins) {
        auto plugin_job = plugin->on_static_opaque_triangles_updated(&base_render_data,
            info,
            render_objects->static_opaque_vertices,
            render_objects->static_opaque_indices,
            render_objects->static_opaque_obj_ids);
        if (plugin_job) {
            register_gpu_job(std::move(plugin_job));
        }
    }

    // Publish the buffers under the global resource IDs so render tasks in the
    // active graph can resolve them without per-pipeline registration.
    set_graph_static_opaque_resources(active_graph);
}

} // namespace WhiteBirdEngine
