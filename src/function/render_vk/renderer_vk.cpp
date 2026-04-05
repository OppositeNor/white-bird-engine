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
#include "core/engine_core.hh"
#include "core/job/job_buffer_ring_spsc.hh"
#include "core/job/job.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/engine_function.hh"
#include "function/function_utils.hh"
#include "function/render/render_database.hh"
#include "function/render/render_task.hh"
#include "function/render/i_renderer.hh"
#include "function/render_vk/allocator_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/ro_staging_buffer_vk.hh"
#include "function/render_vk/rt_pre_execute_pipeline_vk.hh"
#include "function/render_vk/material_vk.hh"
#include "function/window/i_window.hh"
#include "generated/label_manager.gen.hh"
#include "generated/type_uuid.gen.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RendererVK::RendererVK(AllocType* p_allocator, Ref<IWindow> p_window, Ref<JobBufferRingSPSC> p_job_buffer)
    : job_buffer(p_job_buffer), allocator(p_allocator),
    render_database(make_ref<RenderDatabase>(p_allocator, p_allocator, make_unique<MaterialFactoryVK>(allocator))),
    allocator_vk(make_ref<AllocatorVK>(p_allocator, p_allocator)), window(p_window), in_flight_fences(p_allocator),
    base_render_data(p_allocator), pipelines(p_allocator), render_resources(p_allocator), per_frame_in_flight_resource(p_allocator),
    activated_pipeline(p_allocator) {
    WBE_DEBUG_ASSERT(p_job_buffer != MEM_NULL);
    WBE_DEBUG_ASSERT(p_window != MEM_NULL);
    WBE_DEBUG_ASSERT(p_allocator != nullptr);
    singleton = this;
    for (auto& delayed_call : delayed_calls) {
        delayed_call.second = -1;
    }
    init();
}

RendererVK::~RendererVK() {
    terminate();
    singleton = nullptr;
}

void RendererVK::tick(double p_delta) {
    WBE_DEBUG_ASSERT(!(this_ref.is_null()));

    // Run some jobs stored in the job buffer.
    for (size_t i = 0; i < RUN_MAX_JOBS_PER_FRAME; ++i) {
        Ref<Job> job = job_buffer->retrieve_job();
        if (job == MEM_NULL) {
            break;
        }
        job->perform();
    }

    Vector<Ref<PipelineInfo>> run_pipeline = activated_pipeline;
    if (run_pipeline.empty()) {
        stdout_log(WBE_CHANNEL_RENDER)->warning("No pipeline activated. Skipping renderer update.");
        return;
    }

    static uint32_t frame_counter = 0;
    frame_counter = (frame_counter + 1) % frames_in_flight;
    base_render_data.in_flight_fence = in_flight_fences[frame_counter];

    vkWaitForFences(logical_device, 1, &(base_render_data.in_flight_fence), VK_TRUE, UINT64_MAX);

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

    // Aquire swap chain image.
    uint32_t image_index = 0;
    auto* image_available_semaphore = run_pipeline[frame_counter]->image_available_semaphore->get_semaphore();
    VkResult result = vkAcquireNextImageKHR(logical_device, swap_chain, UINT64_MAX,
                                            image_available_semaphore,
                                            VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_swap_chain();
        return;
    }
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image.");
    }

    vkResetFences(logical_device, 1, &(base_render_data.in_flight_fence));
    render_objects->swap_chain_framebuffer_resource->res = render_objects->swap_chain_framebuffers[image_index];
    auto curr_pipeline = run_pipeline[frame_counter];

    for (auto& task_info : curr_pipeline->task_infos) {
        if (task_info.render_task == MEM_NULL) {
            stdout_log(WBE_CHANNEL_RENDER)->warning(
                std::format("Render task {} is NULL. Skipped.", LabelManager::get_singleton()->get_label_name(task_info.task_id)));
            continue;
        }
        task_info.render_task->perform(p_delta);
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = curr_pipeline->result_semaphores.size();
    Vector<VkSemaphore> result_semaphores(allocator);
    result_semaphores.reserve(curr_pipeline->result_semaphores.size());
    for (auto& semaphore : curr_pipeline->result_semaphores) {
        result_semaphores.push_back(semaphore->get_semaphore());
    }
    present_info.pWaitSemaphores = result_semaphores.data();

    VkSwapchainKHR swap_chains[] = {swap_chain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;

    present_info.pResults = nullptr;
    vkQueuePresentKHR(present_queue, &present_info);

    // Clear pre-execute tasks queue.
    base_render_data.pre_execute_tasks_queue.clear();

    ++(base_render_data.current_frame);
}

Ref<RendResVK> RendererVK::get_resource(RendResID p_res_id, RendResTID p_res_type_id) {
    if (!LabelManager::get_singleton()->is_registered(p_res_id)) {
        throw std::runtime_error(std::format("Render resource ID {} does not exist.", p_res_id));
    }
    if (!LabelManager::get_singleton()->is_registered(p_res_type_id)) {
        throw std::runtime_error(std::format("Render resource type ID {} does not exist.", p_res_type_id));
    }
    auto iter = render_resources.find(p_res_id);
    if (iter == render_resources.end()) {
        // Does not exists in regular resources, check per-frame-in-flight resources.
        auto iter_pfif = per_frame_in_flight_resource.find(p_res_id);
        if (iter_pfif == per_frame_in_flight_resource.end()) {
            // Both regular and per-frame-in-flight resources do not have the resource.
            throw std::runtime_error(std::format("Render resource ID {} not found.", p_res_id));
        }
        // Get the resource of the current frame.
        uint32_t frame_index = base_render_data.current_swap_chain_index;
        auto& pfif_resources = iter_pfif->second;
        return pfif_resources[frame_index].lock();
    }
    // Exists in regular resources, return directly.
    return iter->second.lock();
}

template <typename T>
static void update_buffer(const BaseRenderDataVK* p_render_data, DeviceDomain p_domain,
                          Ref<ROBufferVK>& p_buffer, const Vector<T>& p_from_buffer, VkBufferUsageFlags p_buffer_usage,
                          Ref<ROStagingBufferVK> p_staging_buffer) {
    size_t size_in_bytes = p_from_buffer.size() * sizeof(T);
    if (p_buffer == MEM_NULL || size_in_bytes > p_buffer->fields().buffer_size) {
        // If res not initialized, or size exceeds buffer capacity, create a new buffer.
        ROBufferVK::Builder builder;
        builder.set_domain(p_domain)
            .set_usage(p_buffer_usage)
            .set_size(size_in_bytes);
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
            temp_buffer_builder.set_domain(DeviceDomain::HOST)
                .add_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                .set_size(size_in_bytes);
            void* data = p_staging_buffer->allocate(size_in_bytes);
            memcpy(data, p_from_buffer.data(), size_in_bytes);
            p_buffer->update_buffer(p_staging_buffer, data, size_in_bytes);
        } break;
    }
}

void RendererVK::update_static_opaque_triangles() {
    // Vertices
    const auto& static_opaque_triangles_vertices = render_database->get_static_opaque_triangles_info().vertices;
    auto vertex_buffer = render_objects->static_opaque_triangles_buffer_vertices_resource->res.dynamic_cast_ref<ROBufferVK>();
    update_buffer(&base_render_data, DeviceDomain::DEVICE, vertex_buffer, static_opaque_triangles_vertices,
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, render_objects->staging_buffer);
    render_objects->static_opaque_triangles_buffer_vertices_resource->res = vertex_buffer;

    // Indices
    const auto& static_opaque_triangles_indices = render_database->get_static_opaque_triangles_info().indices;
    auto index_buffer = render_objects->static_opaque_triangles_buffer_indices_resource->res.dynamic_cast_ref<ROBufferVK>();
    update_buffer(&base_render_data, DeviceDomain::DEVICE, index_buffer, static_opaque_triangles_indices,
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, render_objects->staging_buffer);
    render_objects->static_opaque_triangles_buffer_indices_resource->res = index_buffer;

    // Object IDs
    const auto& static_opaque_triangles_obj_ids = render_database->get_static_opaque_triangles_info().obj_id;
    auto obj_ids_buffer = render_objects->static_opaque_triangles_buffer_obj_ids_resource->res.dynamic_cast_ref<ROBufferVK>();
    update_buffer(&base_render_data, DeviceDomain::DEVICE, obj_ids_buffer, static_opaque_triangles_obj_ids,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, render_objects->staging_buffer);
    render_objects->static_opaque_triangles_buffer_obj_ids_resource->res = obj_ids_buffer;
}

void RendererVK::init() {
    stdout_log(WBE_CHANNEL_DEBUG)->message("Initializing Vulkan renderer.");
    render_objects = make_unique<RenderObjects>(allocator, allocator);
    init_vulkan();
    setup_base_render_data();
    init_swap_chain_image_views();
    init_render_pass();
    init_swap_chain_framebuffers();
    setup_render_resource();
    stdout_log(WBE_CHANNEL_DEBUG)->message("Vulkan renderer initialized.");
}

void RendererVK::terminate() {
    vkDeviceWaitIdle(logical_device);
    pipelines.clear();
    activated_pipeline.clear();
    render_objects.reset();

    for (auto& delayed_call : delayed_calls) {
        // Call all delayed calls before terminating
        if (delayed_call.second >= 0) {
            delayed_call.first();
        }
    }
    for (auto* fence : in_flight_fences) {
        vkDestroyFence(logical_device, fence, get_allocation_callbacks());
    }
    vkDestroyDescriptorPool(logical_device, descriptor_pool, get_allocation_callbacks());
    vkDestroyCommandPool(logical_device, compute_command_pool, get_allocation_callbacks());
    vkDestroyCommandPool(logical_device, graphics_command_pool, get_allocation_callbacks());
    vkDestroySwapchainKHR(logical_device, swap_chain, get_allocation_callbacks());
    vkDestroySurfaceKHR(vk_instance, surface, nullptr);
    vkDestroyDevice(logical_device, get_allocation_callbacks());
    vkDestroyInstance(vk_instance, get_allocation_callbacks());
}

void RendererVK::init_vulkan() {
    create_instance();
    create_device();
    create_swap_chain();
    create_command_pools();
    create_descriptor_pool();
    create_async_objects();
}

void RendererVK::setup_resource(RendResID p_res_id, RendResTID p_res_type, Ref<RendResVK>& p_res, Ref<RenderObjectVK> p_default_object) {
    WBE_DEBUG_ASSERT(p_res == MEM_NULL);
    WBE_DEBUG_ASSERT(LabelManager::get_singleton()->is_registered(p_res_id));
    WBE_DEBUG_ASSERT(TypeUUIDManager::get_singleton()->is_registered(p_res_type));
    p_res = make_ref<RendResVK>(allocator);
    p_res->res_type_id = p_res_type;
    p_res->res_id = p_res_id;
    p_res->res = p_default_object;
    render_resources.insert({p_res_id, p_res});
}

void RendererVK::recreate_swap_chain() {
    vkDeviceWaitIdle(logical_device);
    clean_up_swap_chain();
    create_swap_chain();
    init_swap_chain_image_views();
    init_swap_chain_framebuffers();
    setup_base_render_data();
}

void RendererVK::setup_base_render_data() {
    base_render_data.allocator = allocator;
    base_render_data.render_database = render_database.get();
    base_render_data.logic_device = logical_device;
    base_render_data.swap_chain_format = swap_chain_image_format;
    base_render_data.swap_chain_extent = swap_chain_extent;
    base_render_data.swap_chain_aspect_ratio
        = static_cast<float>(swap_chain_extent.width) / static_cast<float>(swap_chain_extent.height);
    base_render_data.allocation_callbacks = get_allocation_callbacks();
    base_render_data.graphics_queue = graphics_queue;
    base_render_data.compute_queue = compute_queue;
    base_render_data.viewport = viewport;
    base_render_data.scissor = scissor;
    base_render_data.graphics_command_pool = graphics_command_pool;
    base_render_data.compute_command_pool = compute_command_pool;
    base_render_data.descriptor_pool = descriptor_pool;
    base_render_data.current_frame = 1;
    if (!in_flight_fences.empty()) {
        base_render_data.in_flight_fence = in_flight_fences[0];
    }
    else {
        base_render_data.in_flight_fence = VK_NULL_HANDLE;
    }
    base_render_data.current_swap_chain_index = 0;
    base_render_data.queue_family_indices = queue_family_indices.get_unique_indices();
    base_render_data.find_memory_type = [this](uint32_t p_type_filter, DeviceDomain p_properties) -> uint32_t {
        return this->find_memory_type(p_type_filter, p_properties);
    };
    base_render_data.register_delayed_call = [this](std::function<void()>&& p_function, uint32_t p_delayed_frames) {
        register_delay_call(std::move(p_function), p_delayed_frames);
    };
    base_render_data.frames_in_flight = frames_in_flight;
}

void RendererVK::init_swap_chain_framebuffers() {
    render_objects->swap_chain_framebuffers.resize(swap_chain_images_count);
    // Setup the framebuffer for each swap chain image view.
    for (size_t i = 0; i < swap_chain_images_count; ++i) {
        ROFramebufferVK::Builder builder;
        builder.add_attachment(render_objects->swap_chain_image_views[i])
            .set_render_pass(render_objects->result_render_pass)
            .set_extent(swap_chain_extent)
            .set_layers(1);
        render_objects->swap_chain_framebuffers[i] = builder.create_ref(&base_render_data);
    }
}

void RendererVK::setup_render_resource() {
    ROStagingBufferVK::Builder staging_buffer_builder;
    staging_buffer_builder.set_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT).set_size(STAGING_BUFFER_SIZE);
    render_objects->staging_buffer = staging_buffer_builder.create_ref(&base_render_data);
    // Setup swap_chain_framebuffer_resource.
    setup_resource(REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK, TypeUUID<ROFramebufferVK>::UUID,
                   render_objects->swap_chain_framebuffer_resource, MEM_NULL);
    // Setup static opaque triangles vertices buffer resource.
    setup_resource(REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK, TypeUUID<ROBufferVK>::UUID,
                   render_objects->static_opaque_triangles_buffer_vertices_resource, MEM_NULL);
    // Setup static opaque triangles indices buffer resource.
    setup_resource(REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK, TypeUUID<ROBufferVK>::UUID,
                   render_objects->static_opaque_triangles_buffer_indices_resource, MEM_NULL);
    // Setup static opaque triangles object ids buffer resource.
    setup_resource(REND_RES_STATIC_OPAQUE_TRIANGLES_OBJ_IDS_VK, TypeUUID<ROBufferVK>::UUID,
                   render_objects->static_opaque_triangles_buffer_obj_ids_resource, MEM_NULL);
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

    std::set<std::string> available_extensions;
    uint32_t available_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extension_properties(available_extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count, available_extension_properties.data());
    for (const auto& extension : available_extension_properties) {
        available_extensions.insert(extension.extensionName);
    }

    for (const char* layer_name : enabled_layers) {
        uint32_t layer_extension_count = 0;
        VkResult layer_ext_result = vkEnumerateInstanceExtensionProperties(layer_name, &layer_extension_count, nullptr);
        if (layer_ext_result != VK_SUCCESS) {
            throw std::runtime_error(std::format("Failed to enumerate instance extensions for layer {}.", layer_name));
        }
        std::vector<VkExtensionProperties> layer_extension_properties(layer_extension_count);
        layer_ext_result = vkEnumerateInstanceExtensionProperties(layer_name, &layer_extension_count, layer_extension_properties.data());
        if (layer_ext_result != VK_SUCCESS) {
            throw std::runtime_error(std::format("Failed to enumerate instance extensions for layer {}.", layer_name));
        }
        for (const auto& extension : layer_extension_properties) {
            available_extensions.insert(extension.extensionName);
        }
    }

    // Setup and validate instance extensions.
    std::vector<const char*> required_extensions = window->get_required_vulkan_instance_extensions();
    required_extensions.reserve(required_extensions.size() + enabled_instance_extensions.size());
    required_extensions.insert(required_extensions.end(), enabled_instance_extensions.begin(), enabled_instance_extensions.end());

    std::set<std::string> missing_extensions;
    for (const char* extension_name : required_extensions) {
        if (!available_extensions.contains(extension_name)) {
            missing_extensions.insert(extension_name);
        }
    }
    if (!missing_extensions.empty()) {
        std::string missing_extensions_message;
        for (const auto& missing_extension : missing_extensions) {
            if (!missing_extensions_message.empty()) {
                missing_extensions_message += ", ";
            }
            missing_extensions_message += missing_extension;
        }
        throw std::runtime_error(std::format("Missing required Vulkan instance extension(s): {}.", missing_extensions_message));
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

void RendererVK::create_device() {
    create_physical_device();
    create_logical_device();
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
    }
    else {
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

    // If we have to use the image between multiple queue families, we have to specify it.
    if (unique_indices.size() > 1) {
        // Images can share ownership. If other queue families want to access we don't have to explicitly transfer ownership.
        Vector<uint32_t> unique_indices_array(unique_indices.begin(), unique_indices.end(), allocator);
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = unique_indices.size();
        create_info.pQueueFamilyIndices = unique_indices_array.data();
    }
    else {
        // Images cannot share ownership. If other queue families want to access we have to explicitly transfer ownership.
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
        stdout_log(WBE_CHANNEL_RENDER)->warning(
            std::format("frames_in_flight is required to be at least number of swap chain images; but get {} frames_in_flight and {} images. "
                        "Resizing frames_in_flight to {}.", frames_in_flight, swap_chain_images_count, swap_chain_images_count));
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
    render_objects->swap_chain_framebuffer_resource->res = MEM_NULL;
    render_objects->swap_chain_framebuffers.clear();
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
            .set_components(
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY)
            .set_subresource_range(subresource_range);

        render_objects->swap_chain_image_views.emplace_back(builder.create_ref(&base_render_data));
    }
}

void RendererVK::create_command_pools() {
    VkCommandPoolCreateInfo graphics_pool_create_info{};
    graphics_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    graphics_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    graphics_pool_create_info.queueFamilyIndex = queue_family_indices.graphics.value();
    if (vkCreateCommandPool(logical_device, &graphics_pool_create_info, get_allocation_callbacks(), &graphics_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool.");
    }
    VkCommandPoolCreateInfo compute_pool_create_info{};
    compute_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    compute_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    compute_pool_create_info.queueFamilyIndex = queue_family_indices.graphics.value();
    if (vkCreateCommandPool(logical_device, &compute_pool_create_info, get_allocation_callbacks(), &compute_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute command pool.");
    }
}

void RendererVK::create_descriptor_pool() {
    // uniform buffer descriptor pool
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = MAX_UNIFORM_DESCRIPTOR_COUNT;

    VkDescriptorPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    create_info.maxSets = MAX_DESCRIPTOR_SET_COUNT;
    create_info.poolSizeCount = 1;
    create_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(logical_device, &create_info, get_allocation_callbacks(), &descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool.");
    }
}

void RendererVK::create_async_objects() {
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Set the initial state of the fence to "signaled", since we don't have a "previous frame" at the first frame.
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

void RendererVK::init_render_pass() {
    // Setup color attachment
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swap_chain_image_format;
    // Disable multisample.
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // Clear the color before every render pass.
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // Store the color after every render pass (to render it on the viewport).
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // Don't care about the stencil images.
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // The attachment's initial layout is not known.
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // The attachment's final layout should be able to present on screen.
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Color attachment.
    VkAttachmentReference color_attachment_reference{};
    color_attachment_reference.attachment = 0;
    // Image going to be rendered using a graphics pipeline.
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    RORenderPassVK::Builder render_pass_builder;
    render_pass_builder.add_attachment(color_attachment).add_subpass(subpass).add_dependency(dependency);
    render_objects->result_render_pass = render_pass_builder.create_ref(&base_render_data);
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
    is_physical_device_descrete = device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    String device_name(device_properties.deviceName, allocator);
    stdout_log(WBE_CHANNEL_DEBUG)->message(std::format("Picked video device: \"{}\".", device_name));
    queue_family_indices = get_queue_family_indices(physical_device);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);
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
    create_info.enabledLayerCount = enabled_layers.size();
    create_info.ppEnabledLayerNames = enabled_layers.data();

    // Setup logical device extensions.
    create_info.enabledExtensionCount = enabled_device_extensions.size();
    create_info.ppEnabledExtensionNames = enabled_device_extensions.data();
    // Setup physical device features.
    VkPhysicalDeviceFeatures device_features{};
    device_features.depthClamp = VK_TRUE;
    create_info.pEnabledFeatures = &device_features;

    VkPhysicalDeviceTimelineSemaphoreFeatures semaphore_features{};
    semaphore_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    semaphore_features.timelineSemaphore = VK_TRUE;
    create_info.pNext = &semaphore_features;

    VkPhysicalDeviceSynchronization2Features sync2_features{};
    sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2_features.synchronization2 = VK_TRUE;
    semaphore_features.pNext = &sync2_features;

    // Create logical device.
    if (vkCreateDevice(physical_device, &create_info, get_allocation_callbacks(), &logical_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }

    // Get the device queues.
    vkGetDeviceQueue(logical_device, queue_family_indices.graphics.value(), 0, &graphics_queue);
    vkGetDeviceQueue(logical_device, queue_family_indices.compute.value(), 0, &compute_queue);
    vkGetDeviceQueue(logical_device, queue_family_indices.present.value(), 0, &present_queue);
}

bool RendererVK::check_device_extension_support(VkPhysicalDevice p_device) {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(p_device, nullptr, &extension_count, nullptr);
    Vector<VkExtensionProperties> extension_properties(extension_count, allocator);
    vkEnumerateDeviceExtensionProperties(p_device, nullptr, &extension_count, extension_properties.data());
    std::set<std::string> enabled_extensions(enabled_device_extensions.begin(), enabled_device_extensions.end());
    for (const auto& extension : extension_properties) {
        enabled_extensions.erase(extension.extensionName);
    }
    return enabled_extensions.empty();
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
    int32_t pick_device_index = game_config.render_device_index;
    if (pick_device_index >= 0) {
        // The refered device index is provided, use the index to pick device. If failed to pick, fall back to automatic picking.
        if (std::cmp_greater_equal(pick_device_index ,p_physical_devices.size())) {
            // Index out of range.
            stdout_log(WBE_CHANNEL_DEBUG)->warning(std::format(
                "Failed to use the device provided by the configuration: Index {} out of range (total devices: {}). Picking"
                " an alternative device.", pick_device_index, p_physical_devices.size()));
        }
        else if (!is_device_suitable(p_physical_devices[pick_device_index])) {
            // Device is not suitable.
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(p_physical_devices[pick_device_index], &device_properties);
            String device_name(device_properties.deviceName, allocator);
            stdout_log(WBE_CHANNEL_DEBUG)->warning(std::format(
                "Failed to use the device provided by the configuration: device \"{}\" is not suitable."
                " Picking an alternative device.", device_name));
        }
        else {
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
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(p_device, &device_properties);
    if (device_properties.apiVersion < VULKAN_API_VERSION) {
        return false;
    }
    QueueFamilyIndices device_indices = get_queue_family_indices(p_device);
    if (!device_indices.is_complete()) {
        return false;
    }
    if (!check_device_extension_support(p_device)) {
        return false;
    }
    SwapChainSupportDetails swap_chain_support_details = get_swap_chain_support_details(p_device);
    if (swap_chain_support_details.formats.empty() || swap_chain_support_details.present_modes.empty()) {
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
    int i = 0;
    for (auto& queue_family_property : queue_family_properties) {
        if (queue_family_property.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }
        if (queue_family_property.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.compute = i;
        }
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface, &present_support);
        if (present_support) {
            indices.present = i;
        }
        if (indices.is_complete()) {
            break;
        }
        ++i;
    }
    return indices;
}

RendererVK::SwapChainSupportDetails RendererVK::get_swap_chain_support_details(VkPhysicalDevice p_device) {
    SwapChainSupportDetails details{.formats = Vector<VkSurfaceFormatKHR>(allocator),
        .present_modes = Vector<VkPresentModeKHR>(allocator)};
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
        if (available_format.format == VK_FORMAT_R8G8B8A8_SRGB
            && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
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
    VkExtent2D result {
        .width = extent_width,
        .height = extent_height
    };
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

RendererVK::Vector<Ref<RendererVK::PipelineInfo>> RendererVK::compile_pipeline(const std::string& p_pipeline_name, const RendPipelineInfo& p_pipeline_info) {
    if (p_pipeline_info.root_task == MEM_NULL) {
        throw std::runtime_error("Root task of a render pipeline cannot be NULL.");
    }
    if (!p_pipeline_info.root_task->prev.empty()) {
        throw std::runtime_error("The first task of the render pipeline must not have any previous tasks.");
    }
    Vector<Ref<RendererVK::PipelineInfo>> pipeline_infos(allocator);

    pipeline_infos.reserve(frames_in_flight);
    for (size_t i = 0; i < frames_in_flight; ++i) {
        // Copy frames_in_flight amount of pipelines for each swap chain image to draw without waiting for the previous one to finish.
        auto result_pipeline_info = make_ref<PipelineInfo>(allocator, allocator);
        result_pipeline_info->pipeline_name = p_pipeline_name;
        compile_tasks(p_pipeline_info.root_task, p_pipeline_info, result_pipeline_info);
        pipeline_infos.emplace_back(result_pipeline_info);
    }
    return pipeline_infos;
}

void RendererVK::compile_tasks(Ref<RendTaskInfo> p_root_task, const RendPipelineInfo& p_entered_pipeline_info, Ref<PipelineInfo> p_pipeline_info) {
    WBE_DEBUG_ASSERT(p_root_task != MEM_NULL);
    WBE_DEBUG_ASSERT(p_pipeline_info != MEM_NULL);
    // Using bfs, as the order where the render task will be pushed to the queue.
    std::map<RendTaskID, std::pair<Ref<RenderTaskVK>, const std::vector<RendTaskID>*>> render_task_map;
    // Setup image available semaphores
    auto image_available_semaphore = make_ref<SemaphoreVK>(allocator, &base_render_data, true);
    p_pipeline_info->image_available_semaphore = image_available_semaphore;
    // Register pipeline resources.
    register_pipeline_resources(p_pipeline_info, p_entered_pipeline_info.resources);
    // Setup start task.
    Ref<RTPreExecutePipelineFactoryVK> start_task_factory
        = make_ref<RTPreExecutePipelineFactoryVK>(allocator, &base_render_data, image_available_semaphore);
    Ref<RenderTaskVK> start_task = start_task_factory->create().dynamic_cast_ref<RenderTaskVK>();
    start_task->task_semaphore = make_ref<SemaphoreVK>(allocator, &base_render_data, true);
    p_pipeline_info->task_infos.push_back({.task_id = RT_PRE_EXECUTE_PIPELINE_VK, .render_task = start_task});
    std::vector<RendTaskID> dummy;
    render_task_map.insert({RT_PRE_EXECUTE_PIPELINE_VK, {start_task, &dummy}});
    // Compile the render graph starting from the root task.
    compile_render_graph(p_root_task, p_pipeline_info, render_task_map);

    // Setup other properties according to the constructed graph.
    uint64_t max_depth = 0;
    for (auto rend_task : p_pipeline_info->task_infos) {
        const auto* dep_tasks = render_task_map[rend_task.task_id].second;
        for (auto dep_task_id : *dep_tasks) {
            // Setup tasks semaphore dependencies.
            auto dep_task = render_task_map[dep_task_id].first;
            rend_task.render_task->depend_semaphores.push_back(dep_task->task_semaphore);
            // This was originally implemented for something else that was removed,
            // but this might be useful in the future, so I'll keep it here.
            rend_task.render_task->node_depth = std::max(rend_task.render_task->node_depth, dep_task->node_depth + 1);
        }
        max_depth = std::max(rend_task.render_task->node_depth, max_depth);
    }
    p_pipeline_info->max_depth = max_depth;

    // Initialize the tasks.
    for (auto rend_task : p_pipeline_info->task_infos) {
        rend_task.render_task->setup();
    }
}

void RendererVK::compile_render_graph(Ref<RendTaskInfo> p_root_task, Ref<PipelineInfo> p_pipeline_info,
                                      std::map<RendTaskID, std::pair<Ref<RenderTaskVK>, const std::vector<RendTaskID>*>>& p_render_task_map) {
    WBE_DEBUG_ASSERT(p_root_task != MEM_NULL);
    WBE_DEBUG_ASSERT(p_pipeline_info != MEM_NULL);
    // Process the tasks with bfs, which will also be the order of the tasks that will be sumitted to the queue.
    // Performing BFS should be the most optimal way for optimizing synchronozation, since the nodes with the same
    // depth has no dependencies among them.
    Deque<Ref<RendTaskInfo>> to_visit(allocator);
    to_visit.push_back(p_root_task);
    HashSet<Ref<RendTaskInfo>> visited(allocator);
    while (!to_visit.empty()) {
        Ref<RendTaskInfo> curr = to_visit.front();
        to_visit.pop_front();
        if (visited.contains(curr)) {
            continue;
        }
        visited.insert(curr);
        for (const auto& next : curr->next) {
            to_visit.push_back(next);
        }

        TaskInfo task_info;
        // Set render task.
        task_info.render_task = curr->render_task_factory->create().dynamic_cast_ref<RenderTaskVK>();
        if (task_info.render_task == MEM_NULL) {
            throw std::runtime_error(std::format(
                "Failed to convert RenderTask {} to RenderTaskVK. This task is probably not a Vulkan task.",
                LabelManager::get_singleton()->get_label_name(curr->task_id)));
        }

        // Set task ID.
        task_info.task_id = curr->task_id;
        // Register associated resources.
        fill_task_render_resource(curr, task_info, p_pipeline_info);
        // Push the node to the pipeline.
        p_pipeline_info->task_infos.push_back(task_info);

        static const std::vector<RendTaskID> root_prev_task = {RT_PRE_EXECUTE_PIPELINE_VK};
        if (curr == p_root_task) {
            // If it is the root task, the previous task should be RT_PRE_EXECUTE_PIPELINE_VK.
            p_render_task_map.insert({task_info.task_id, {task_info.render_task, &root_prev_task}});
        }
        else {
            // Keep track on the tasks' dependencies.
            p_render_task_map.insert({task_info.task_id, {task_info.render_task, &(curr->prev)}});
        }
        // Initializes semaphore. If more than one task depend on this task, create a timeline semaphore. Otherwise use a binary semaphore.
        task_info.render_task->task_semaphore = make_ref<SemaphoreVK>(base_render_data.allocator, &base_render_data, curr->next.size() <= 1);
        if (curr->next.empty()) {
            // If this is the last task, keep track on their semaphores, to track when the tasks finish.
            p_pipeline_info->result_semaphores.push_back(task_info.render_task->task_semaphore);
        }
    }
}

Ref<RendResVK> RendererVK::get_rend_res(RendResID p_res, RendResTID p_type_id) {
    WBE_DEBUG_ASSERT(LabelManager::get_singleton()->is_registered(p_res));
    WBE_DEBUG_ASSERT(TypeUUIDManager::get_singleton()->is_registered(p_type_id));

    // Remove the per frame in flight resource if exists.
    per_frame_in_flight_resource.erase(p_res);

    auto iter = render_resources.find(p_res);
    if (iter == render_resources.end() || !iter->second.is_valid()) {
        auto rend_res = make_ref<RendResVK>(allocator);
        rend_res->res_type_id = p_type_id;
        rend_res->res_id = p_res;
        rend_res->res = MEM_NULL;
        render_resources.insert({p_res, rend_res});
        return rend_res;
    }
    WBE_DEBUG_ASSERT(iter->second.lock()->res_type_id == p_type_id);
    return iter->second.lock();
}

Vector<Ref<RendResVK>> RendererVK::get_per_frame_in_flight_rend_res(RendResID p_res, RendResTID p_type_id) {
    WBE_DEBUG_ASSERT(LabelManager::get_singleton()->is_registered(p_res));
    WBE_DEBUG_ASSERT(LabelManager::get_singleton()->is_registered(p_type_id));

    // Remove the regular render resource if exists.
    render_resources.erase(p_res);

    auto iter = per_frame_in_flight_resource.find(p_res);
    if (iter == per_frame_in_flight_resource.end()) {
        // If not found, create a new array of resources.
        Vector<RefWeak<RendResVK>> frame_resources(allocator);
        frame_resources.reserve(frames_in_flight);
        for (size_t i = 0; i < frames_in_flight; ++i) {
            // For each frame in flight, create a resource reference.
            auto rend_res = make_ref<RendResVK>(allocator);
            rend_res->res_type_id = p_type_id;
            rend_res->res_id = p_res;
            rend_res->res = MEM_NULL;
            frame_resources.emplace_back(rend_res);
        }
        per_frame_in_flight_resource.insert({p_res, std::move(frame_resources)});
        iter = per_frame_in_flight_resource.find(p_res);
    }
    Vector<Ref<RendResVK>> result(allocator);
    result.reserve(frames_in_flight);
    for (auto& res : iter->second) {
        result.push_back(res.lock());
    }
    return result;
}

void RendererVK::fill_task_render_resource(Ref<RendTaskInfo> p_task, TaskInfo& p_task_info, Ref<PipelineInfo> p_pipeline) {
    WBE_DEBUG_ASSERT(p_task != MEM_NULL);
    WBE_DEBUG_ASSERT(p_pipeline != MEM_NULL);
    WBE_DEBUG_ASSERT(p_task_info.render_task->req_rend_res.empty());
    WBE_DEBUG_ASSERT(LabelManager::get_singleton()->is_registered(p_task->task_id));
    p_task_info.render_task->req_rend_res.reserve(p_task->req_res.size());
    for (auto& res : p_task->req_res) {
        auto iter = render_resources.find(res.res_id);
        if (iter == render_resources.end() || !iter->second.is_valid()) {
            throw std::runtime_error(std::format(
                "Render resource {} required by task {} is not registered.",
                LabelManager::get_singleton()->get_label_name(res.res_id),
                LabelManager::get_singleton()->get_label_name(p_task->task_id)));
        }
        Ref<RendResVK> rend_res = iter->second.lock();
        p_task_info.render_task->req_rend_res.push_back(rend_res);
    }
}

void RendererVK::register_pipeline_resources(Ref<PipelineInfo> p_pipeline, std::span<const RendPipelineInfo::Resource> p_pipeline_resources) {
    WBE_DEBUG_ASSERT(p_pipeline != MEM_NULL);
    WBE_DEBUG_ASSERT(p_pipeline->rend_res.empty());
    p_pipeline->rend_res.reserve(p_pipeline_resources.size());
    for (const auto& pipeline_resource : p_pipeline_resources) {
        Ref<RendResVK> rend_res = get_rend_res(pipeline_resource.res_id, pipeline_resource.type_id);
        if (pipeline_resource.is_weak) {
            // Weak reference, only set if the resource is not initialized yet.
            if (rend_res->res == MEM_NULL && pipeline_resource.res == MEM_NULL) {
                throw std::runtime_error(std::format(
                    "Render resource {} is a weak reference that is MEM_NULL, and there was no corresponding registered resource.",
                    LabelManager::get_singleton()->get_label_name(pipeline_resource.res_id)));
            }
            rend_res->res = pipeline_resource.res.dynamic_cast_ref<RenderObjectVK>();
            WBE_DEBUG_ASSERT(rend_res->res != MEM_NULL);
        }
        else {
            // Strong reference, always overwrite the resource.
            if (pipeline_resource.res == MEM_NULL) {
                throw std::runtime_error(std::format("Render resource {} is a strong reference that is MEM_NULL.",
                    LabelManager::get_singleton()->get_label_name(pipeline_resource.res_id)));
            }
            rend_res->res = pipeline_resource.res.dynamic_cast_ref<RenderObjectVK>();
            WBE_DEBUG_ASSERT(rend_res->res != MEM_NULL);
        }
        p_pipeline->rend_res.push_back(rend_res);
    }
}

void RendererVK::register_pfif_pipeline_resources(Ref<PipelineInfo> p_pipeline, std::span<const RendPipelineInfo::PFIFResource> p_pipeline_resources) {
    WBE_DEBUG_ASSERT(p_pipeline != MEM_NULL);
    p_pipeline->rend_res.reserve(p_pipeline_resources.size());
    for (const auto& pipeline_pfif_res : p_pipeline_resources) {
        // For each per frame resource, register them into the corresponding entry from get_per_frame_in_flight_rend_res.
        Vector<RefWeak<RendResVK>> frame_resource(allocator);
        frame_resource.reserve(pipeline_pfif_res.res.size());
        auto rend_res = get_per_frame_in_flight_rend_res(pipeline_pfif_res.res_id, pipeline_pfif_res.type_id);
        frame_resource.append_range(rend_res);
        if (pipeline_pfif_res.is_weak) {
            // Weak reference, only set if the resource is not initialized yet.
            if (pipeline_pfif_res.res.empty()) {
                // PFIFResource with empty res are considered NULL.
                if (rend_res[0]->res == MEM_NULL) {
                    // Not registered before, and there is no new instance passed in, throw an error.
                    throw std::runtime_error(std::format(
                        "Render resource {} is a weak reference that is MEM_NULL, and there was no corresponding registered resource.",
                        LabelManager::get_singleton()->get_label_name(pipeline_pfif_res.res_id)));
                }
                continue;
            }
            if (pipeline_pfif_res.res.size() != frames_in_flight) {
                // We passed in instances for weak references, but the number of instance does not match frame_in_flight. Throw an error.
                throw std::runtime_error(std::format(
                    "Failed to register per-frame-in-flight resource: there should be frames_in_flight: {} resource instance registered for each resource,"
                    "But got {} resource instances.",
                    frames_in_flight, pipeline_pfif_res.res.size()));
            }
            for (auto [index, res_to_fill] : std::views::enumerate(rend_res)) {
                // Fill in the resources.
                res_to_fill->res = pipeline_pfif_res.res[index].dynamic_cast_ref<RenderObjectVK>();
                WBE_DEBUG_ASSERT(!res_to_fill->res.is_null());
            }
        }
        else {
            // Strong reference, replace directly.
            if (pipeline_pfif_res.res.empty()) {
                // Strong reference should not be NULL.
                throw std::runtime_error(std::format("Render resource {} is a strong reference that is MEM_NULL.",
                    LabelManager::get_singleton()->get_label_name(pipeline_pfif_res.res_id)));
            }
            if (pipeline_pfif_res.res.size() != frames_in_flight) {
                // The number of resources does not match frames_in_flight. Throw an error.
                throw std::runtime_error(std::format(
                    "Failed to register per-frame-in-flight resource: there should be frames_in_flight: {} resource instance registered for each resource,"
                    "But got {} resource instances.",
                    frames_in_flight, pipeline_pfif_res.res.size()));
            }
            for (auto [index, res_to_fill] : std::views::enumerate(rend_res)) {
                // Fill in the resources.
                res_to_fill->res = pipeline_pfif_res.res[index].dynamic_cast_ref<RenderObjectVK>();
                WBE_DEBUG_ASSERT(res_to_fill->res != MEM_NULL);
            }
        }
        for (auto& pfif_res : rend_res) {
            // Fill all the resources in the pipeline's rend_res.
            p_pipeline->rend_res.push_back(pfif_res);
        }
    }
}


int32_t RendererVK::rate_memory_type(VkMemoryType p_memory_type, DeviceDomain p_domain) const {
    switch (p_domain) {
        case DeviceDomain::HOST:
            if (!(p_memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                && p_memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                return -1; // not a suitable memory property.
            }
            if (p_memory_type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                return 50; // not exclusive for host, but still could be used.
            }
            return 100; // host exclusive, which usually is better than shared.
            break;
        case DeviceDomain::DEVICE:
            if (!(p_memory_type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                return -1; // not a suitable memory property.
            }
            // For discrete GPUs, allocating exclusivly on the device is a better choice.
            // For other types, such as a UMA device, or integrated GPU, allocating on
            // shared memory is a better choice.
            if (p_memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                return is_physical_device_descrete ? 50 : 100;
            }
            return is_physical_device_descrete ? 100 : 50;
            break;
        case DeviceDomain::HOST_DEVICE:
            if (!(p_memory_type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                && p_memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                && p_memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                return -1; // not a suitable memory property.
            }
            return 100;
            break;
    };
    throw std::runtime_error("Unrecognized device domain.");
}

uint32_t RendererVK::find_memory_type(uint32_t p_type_filter, DeviceDomain p_domain) const {
    int32_t best_type = -1;
    int32_t best_type_score = 0;
    for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; ++i) {
        // Find the first memory property that could be used.
        if (!(p_type_filter & (1 << i))) {
            // Not able to be allocated on the current type, continue.
            continue;
        }
        int32_t type_score = rate_memory_type(physical_device_memory_properties.memoryTypes[i], p_domain);
        if (type_score < 0) {
            continue;
        }
        if (type_score > best_type_score) {
            best_type = static_cast<int32_t>(i);
            best_type_score = type_score;
        }
    }
    if (best_type < 0) {
        throw std::runtime_error("Failed to find a suitable physical device to allocate memory.");
    }
    return best_type;
}
} // namespace WhiteBirdEngine
