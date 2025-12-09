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
#include "core/allocator/allocator.hh"
#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/engine_function.hh"
#include "function/render/render_database.hh"
#include "function/render/render_task.hh"
#include "function/render/render_utils.hh"
#include "function/render/renderer.hh"
#include "function/render_vk/allocator_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/rt_pre_execute_pipeline_vk.hh"
#include "generated/label_manager.gen.hh"
#include "generated/type_uuid.gen.hh"
#include "global/stl_allocator.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include <SDL_vulkan.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RendererVK::RendererVK(AllocType* p_allocator, Ref<JobBufferRingSPSC<JobRender>> p_job_buffer)
    : job_buffer(p_job_buffer), allocator(p_allocator),
    render_database(make_ref<RenderDatabase>(p_allocator, p_allocator)),
    allocator_vk(make_ref<AllocatorVK>(p_allocator, p_allocator)), swap_chain_images(p_allocator),
    in_flight_fences(p_allocator), base_render_data(p_allocator),
    activated_pipeline(p_allocator) {
    WBE_DEBUG_ASSERT(p_job_buffer != MEM_NULL);
    WBE_DEBUG_ASSERT(p_allocator != nullptr);
    singleton = this;
    init();
}

RendererVK::~RendererVK() {
    terminate();
    singleton = nullptr;
}

void RendererVK::tick(double p_delta) {
    WBE_DEBUG_ASSERT(!(this_ref.is_null()));
    // Clear pre-execute tasks queue.
    base_render_data.pre_execute_tasks_queue.clear();

    // Run some jobs stored in the job buffer.
    for (size_t i = 0; i < RUN_MAX_JOBS_PER_FRAME; ++i) {
        Ref<JobRender> job = job_buffer->retrieve_job();
        if (job == MEM_NULL) {
            break;
        }
        job->perform(this_ref.lock());
    }

    vector<Ref<PipelineInfo>> run_pipeline = activated_pipeline;
    if (run_pipeline.size() == 0) {
        wbe_console_log(WBE_CHANNEL_RENDER)->warning("No pipeline activated. Skipping renderer update.");
        return;
    }

    static uint32_t frame_counter = 0;
    frame_counter = (frame_counter + 1) % frames_in_flight;
    base_render_data.in_flight_fence = in_flight_fences[frame_counter];

    vkWaitForFences(logical_device, 1, &(base_render_data.in_flight_fence), VK_TRUE, UINT64_MAX);

    // Aquire swap chain image.
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(logical_device, swap_chain, UINT64_MAX,
                                            run_pipeline[frame_counter]->image_available_semaphore->get_semaphore(),
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
            wbe_console_log(WBE_CHANNEL_RENDER)->warning(std::format("Render task {} is NULL. Skipped.", task_info.task_id));
            continue;
        }
        task_info.render_task->perform(p_delta);
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = curr_pipeline->result_semaphores.size();
    vector<VkSemaphore> result_semaphores(allocator);
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

    ++(base_render_data.current_frame);
}

void RendererVK::update_static_opaque_triangles() {
    auto& static_opaque_triangles_vertices = render_database->get_static_opaque_triangle_vertices();
    ROBufferVK::Builder builder;
    auto& res = render_objects->static_opaque_triangles_vertices_buffer_resource->res;
    if (render_objects->static_opaque_triangles_vertices_buffer_resource->res != MEM_NULL &&
        static_opaque_triangles_vertices.size() > res.dynamic_cast_ref<ROBufferVK>()->fields().buffer_size) {
        // If size exceeds buffer capacity, create a new buffer.
        builder.set_usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT).set_size(static_opaque_triangles_vertices.size());
        res = make_ref<ROBufferVK>(allocator, &base_render_data, builder);
    }
    res.dynamic_cast_ref<ROBufferVK>()->update_buffer(static_opaque_triangles_vertices.data(),
                                                      static_opaque_triangles_vertices.size() * sizeof(Vertex3DTextured));
}

void RendererVK::pool_events(bool& p_should_close, Ref<InputManager> p_input_manager) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                p_should_close = true;
                break;
            // TODO: update input manager.
        }
    }
}

void RendererVK::init() {
    wbe_console_log(WBE_CHANNEL_DEBUG)->message("Initializing Vulkan renderer.");
    render_objects = make_unique<RenderObjects>(allocator, allocator);
    init_window();
    init_vulkan();
    setup_base_render_data();
    init_swap_chain_image_views();
    init_render_pass();
    init_graphics_pipeline();
    init_swap_chain_framebuffers();
    setup_render_resource();
    wbe_console_log(WBE_CHANNEL_DEBUG)->message("Vulkan renderer initialized.");
}

void RendererVK::terminate() {
    vkDeviceWaitIdle(logical_device);
    pipelines.clear();
    activated_pipeline.clear();
    render_objects.reset();
    for (auto fence : in_flight_fences) {
        vkDestroyFence(logical_device, fence, get_allocation_callbacks());
    }
    vkDestroyCommandPool(logical_device, compute_command_pool, get_allocation_callbacks());
    vkDestroyCommandPool(logical_device, graphics_command_pool, get_allocation_callbacks());
    vkDestroySwapchainKHR(logical_device, swap_chain, get_allocation_callbacks());
    vkDestroySurfaceKHR(vk_instance, surface, nullptr);
    vkDestroyDevice(logical_device, get_allocation_callbacks());
    vkDestroyInstance(vk_instance, get_allocation_callbacks());
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
}

void RendererVK::init_window() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Vulkan_LoadLibrary(nullptr);
    auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    window = SDL_CreateWindow(game_config.game_name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              game_config.window_width, game_config.window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
}

void RendererVK::init_vulkan() {
    create_instance();
    create_window_surface();
    create_device();
    create_swap_chain();
    create_command_pools();
    create_async_objects();
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
    base_render_data.allocation_callbacks = get_allocation_callbacks();
    base_render_data.graphics_queue = graphics_queue;
    base_render_data.compute_queue = compute_queue;
    base_render_data.viewport = viewport;
    base_render_data.scissor = scissor;
    base_render_data.graphics_command_pool = graphics_command_pool;
    base_render_data.compute_command_pool = compute_command_pool;
    base_render_data.current_frame = 1;
    if (in_flight_fences.size() > 0) {
        base_render_data.in_flight_fence = in_flight_fences[0];
    }
    else {
        base_render_data.in_flight_fence = VK_NULL_HANDLE;
    }
    base_render_data.current_swap_chain_index = 0;
    base_render_data.queue_family_indices = queue_family_indices.get_unique_indices();
    base_render_data.find_memory_type = [this](uint32_t p_type_filter, VkMemoryPropertyFlags p_properties) -> uint32_t {
        return this->find_memory_type(p_type_filter, p_properties);
    };
}

void RendererVK::init_graphics_pipeline() {
    // Initialize shaders.
    ROShaderVK::Builder shader_builder;
    RUBinary* shader_code = ResourceManager::get_singleton()->get_resource<RUBinary>("main_shader_vert");
    // TODO: update
    shader_builder.set_code(shader_code->data.get(), shader_code->size);
    Ref<ROShaderVK> vertex_shader = make_ref<ROShaderVK>(allocator, &base_render_data, shader_builder);
    shader_code = ResourceManager::get_singleton()->get_resource<RUBinary>("main_shader_frag");
    shader_builder.set_code(shader_code->data.get(), shader_code->size);
    Ref<ROShaderVK> pixel_shader = make_ref<ROShaderVK>(allocator, &base_render_data, shader_builder);

    // Initialize shader configurations.
    ROGraphicsPipelineVK::Config::ShaderConfig shader_config;
    shader_config
        .push_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader)
        .push_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, pixel_shader);

    // Initialize vertex inputs.
    ROGraphicsPipelineVK::Config::VertexInputConfig vertex_input_config{};
    // TODO: vertex input config

    // Initialize graphics pipeline layout.
    ROPipelineLayoutVK::Builder pipeline_layout_builder;
    render_objects->graphics_pipeline_layout = make_ref<ROPipelineLayoutVK>(allocator, &base_render_data, pipeline_layout_builder);

    // Initialize graphics pipeline.
    auto graphics_pipeline_config = ROGraphicsPipelineVK::get_default_render_opaque_configuration(
        &base_render_data, shader_config, vertex_input_config, render_objects->graphics_pipeline_layout, render_objects->result_render_pass
    );
    render_objects->graphics_pipeline = make_ref<ROGraphicsPipelineVK>(allocator, &base_render_data, graphics_pipeline_config);
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
        render_objects->swap_chain_framebuffers[i] = make_ref<ROFramebufferVK>(allocator, &base_render_data, builder);
    }
}

void RendererVK::setup_render_resource() {
    // Setup swap_chain_framebuffer_resource.
    render_objects->swap_chain_framebuffer_resource = make_ref<RendResVK>(allocator, &base_render_data);
    render_objects->swap_chain_framebuffer_resource->res_type_id = TypeUUID<ROFramebufferVK>::UUID;
    render_objects->swap_chain_framebuffer_resource->res_id = REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK;
    render_objects->swap_chain_framebuffer_resource->res = MEM_NULL;
    render_resources.insert(std::pair(REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK, render_objects->swap_chain_framebuffer_resource));

    // Setup base_graphics_pipeline_resource.
    render_objects->base_graphics_pipeline_resource = make_ref<RendResVK>(allocator, &base_render_data);
    render_objects->base_graphics_pipeline_resource->res_type_id = TypeUUID<ROGraphicsPipelineVK>::UUID;
    render_objects->base_graphics_pipeline_resource->res_id = REND_RES_BASE_GRAPHICS_PIPELINE_VK;
    render_objects->base_graphics_pipeline_resource->res = render_objects->graphics_pipeline;
    render_resources.insert(std::pair(REND_RES_BASE_GRAPHICS_PIPELINE_VK, render_objects->base_graphics_pipeline_resource));

    // Setup static_opaque_triangles_vertices_buffer_resource.
    render_objects->static_opaque_triangles_vertices_buffer_resource = make_ref<RendResVK>(allocator, &base_render_data);
    render_objects->static_opaque_triangles_vertices_buffer_resource->res_type_id = TypeUUID<ROBufferVK>::UUID;
    render_objects->static_opaque_triangles_vertices_buffer_resource->res_id = REND_RES_STATIC_OPAQUE_TRIANGLES_VK;
    render_objects->static_opaque_triangles_vertices_buffer_resource->res = MEM_NULL;
    render_resources.insert(std::pair(REND_RES_STATIC_OPAQUE_TRIANGLES_VK, render_objects->static_opaque_triangles_vertices_buffer_resource));
}

void RendererVK::create_instance() {
    // Get configuration informations.
    auto& engine_config = EngineCore::get_singleton()->engine_config->get_config_options();
    auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();

    // Setup Application info.
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = game_config.game_name.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(game_config.version_major, game_config.version_minor, game_config.version_patch);
    app_info.pEngineName = engine_config.engine_name;
    app_info.engineVersion = VK_MAKE_VERSION(engine_config.version_major, engine_config.version_minor, engine_config.version_patch);
    app_info.apiVersion = VULKAN_API_VERSION;

    // Setup instance create info.
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    // Enable validation layer for debug build.
    if constexpr (enable_validation_layer) {
        create_info.enabledLayerCount = validation_layers.size();
        create_info.ppEnabledLayerNames = validation_layers.data();
    }
    else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }

    // Setup instance extensions.
    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, nullptr);
    vector<const char*> extension_names(extensions_count, allocator);
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extension_names.data());
    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extension_names.data();

    // Create instance.
    if (vkCreateInstance(&create_info, get_allocation_callbacks(), &vk_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance.");
    }
}

void RendererVK::create_window_surface() {
    if (!SDL_Vulkan_CreateSurface(window, vk_instance, &surface)) {
        throw std::runtime_error("Failed to create window surface.");
    }
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
        vector<uint32_t> unique_indices_array(unique_indices.begin(), unique_indices.end(), allocator);
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
    swap_chain_images.resize(swap_chain_images_count);
    vkGetSwapchainImagesKHR(logical_device, swap_chain, &swap_chain_images_count, swap_chain_images.data());
    auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    frames_in_flight = game_config.frames_in_flight;
    if (frames_in_flight < swap_chain_images_count) {
        wbe_console_log(WBE_CHANNEL_RENDER)->warning(
            std::format("frames_in_flight is required to be at least number of swap chain images; but get {} frames_in_flight and {} images. "
                        "Resizing frames_in_flight to {}.", frames_in_flight, swap_chain_images_count, swap_chain_images_count));
    }
    swap_chain_image_format = surface_format.format;
    swap_chain_extent = extent;

    // Setup viewport and scissor information.
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = extent;
}

void RendererVK::clean_up_swap_chain() {
    render_objects->swap_chain_framebuffer_resource->res = MEM_NULL;
    render_objects->swap_chain_framebuffers.clear();
    render_objects->swap_chain_image_views.clear();
    swap_chain_images.clear();
    vkDestroySwapchainKHR(logical_device, swap_chain, get_allocation_callbacks());
}

void RendererVK::init_swap_chain_image_views() {
    render_objects->swap_chain_image_views.reserve(swap_chain_images.size());
    for (size_t i = 0; i < swap_chain_images.size(); ++i) {
        VkImageSubresourceRange subresource_range;
        // Only use the color information of the image.
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Map to a mipmap level, map to the original image, which is 0.
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        ROImageViewVK::Builder builder;
        builder.set_image(swap_chain_images[i])
            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
            .set_format(swap_chain_image_format)
            .set_components(
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY)
        .set_subresource_range(subresource_range);

        render_objects->swap_chain_image_views.push_back(
            make_ref<ROImageViewVK>(allocator, &base_render_data, builder)
        );
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

void RendererVK::create_async_objects() {
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Set the initial state of the fence to "signaled", since we don't have a "previous frame" at the first frame.
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    in_flight_fences.reserve(frames_in_flight);
    for (size_t i = 0; i < frames_in_flight; ++i) {
        VkFence fence;
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
    render_objects->result_render_pass = make_ref<RORenderPassVK>(allocator, &base_render_data, render_pass_builder);
}

void RendererVK::create_physical_device() {
    // Enumerate all physical devices.
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, nullptr);
    if (physical_device_count == 0) {
        throw std::runtime_error("No valid physical device found.");
    }
    vector<VkPhysicalDevice> physical_devices(physical_device_count, allocator);
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, physical_devices.data());

    // Pick one that's the most suitable.
    pick_physical_device(physical_devices);
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    string device_name(device_properties.deviceName, allocator);
    wbe_console_log(WBE_CHANNEL_DEBUG)->message(std::format("Picked device: \"{}\".", device_name));
    queue_family_indices = get_queue_family_indices(physical_device);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);
}

void RendererVK::create_logical_device() {
    // Setup queues create info.
    float queue_priority = 1.0;
    set<uint32_t> indices = queue_family_indices.get_unique_indices();
    vector<VkDeviceQueueCreateInfo> queues_create_info(allocator);
    queues_create_info.reserve(indices.size());
    for (int elem : indices) {
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
    // Enable validation layer for debug builds.
    if constexpr (enable_validation_layer) {
        create_info.enabledLayerCount = validation_layers.size();
        create_info.ppEnabledLayerNames = validation_layers.data();
    }
    else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }
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

    // Create logical device.
    if (vkCreateDevice(physical_device, &create_info, get_allocation_callbacks(), &logical_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }

    // Get the device queues.
    vkGetDeviceQueue(logical_device, queue_family_indices.graphics.value(), 0, &graphics_queue);
    vkGetDeviceQueue(logical_device, queue_family_indices.graphics.value(), 0, &compute_queue);
    vkGetDeviceQueue(logical_device, queue_family_indices.graphics.value(), 0, &present_queue);
}

bool RendererVK::check_device_extension_support(VkPhysicalDevice p_device) {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(p_device, nullptr, &extension_count, nullptr);
    vector<VkExtensionProperties> extension_properties(extension_count, allocator);
    vkEnumerateDeviceExtensionProperties(p_device, nullptr, &extension_count, extension_properties.data());
    std::set<std::string> enabled_extensions(enabled_device_extensions.begin(), enabled_device_extensions.end());
    for (const auto& extension : extension_properties) {
        enabled_extensions.erase(extension.extensionName);
    }
    return enabled_extensions.empty();
}

bool RendererVK::check_validation_layer_support() {
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    vector<VkLayerProperties> layer_properties(layer_count, allocator);
    vkEnumerateInstanceLayerProperties(&layer_count, layer_properties.data());
    for (auto* name : validation_layers) {
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

void RendererVK::pick_physical_device(const vector<VkPhysicalDevice>& p_physical_devices) {
    physical_device = VK_NULL_HANDLE;
    auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    int32_t pick_device_index = game_config.render_device_index;
    if (pick_device_index >= 0) {
        // The refered device index is provided, use the index to pick device. If failed to pick, fall back to automatic picking.
        if (pick_device_index >= static_cast<int32_t>(p_physical_devices.size())) {
            // Index out of range.
            wbe_console_log(WBE_CHANNEL_DEBUG)->warning(std::format(
                "Failed to use the device provided by the configuration: Index {} out of range (total devices: {}). Picking"
                " an alternative device.", pick_device_index, p_physical_devices.size()));
        }
        else if (!is_device_suitable(p_physical_devices[pick_device_index])) {
            // Device is not suitable.
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(p_physical_devices[pick_device_index], &device_properties);
            string device_name(device_properties.deviceName, allocator);
            wbe_console_log(WBE_CHANNEL_DEBUG)->warning(std::format(
                "Failed to use the device provided by the configuration: device \"{}\" is not suitable."
                " Picking an alternative device.", device_name));
        }
        else {
            // Successfully pick the device.
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(p_physical_devices[pick_device_index], &device_properties);
            string device_name(device_properties.deviceName, allocator);
            physical_device = p_physical_devices[pick_device_index];
            wbe_console_log(WBE_CHANNEL_DEBUG)->message(std::format("Picked device: \"{}\".", device_name));
            return;
        }
    }
    // Pick a suitable device.
    for (auto& device : p_physical_devices) {
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
    return true;
}

RendererVK::QueueFamilyIndices RendererVK::get_queue_family_indices(VkPhysicalDevice p_device) {
    QueueFamilyIndices indices{};
    indices.allocator = allocator;
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_family_count, nullptr);
    vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count, allocator);
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
    SwapChainSupportDetails details{.formats = vector<VkSurfaceFormatKHR>(allocator),
        .present_modes = vector<VkPresentModeKHR>(allocator)};
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

VkSurfaceFormatKHR RendererVK::choose_swap_surface_format(const vector<VkSurfaceFormatKHR>& p_available_formats) {
    for (const auto& available_format : p_available_formats) {
        if (available_format.format == VK_FORMAT_R8G8B8A8_SRGB
            && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }
    return p_available_formats[0];
}

VkPresentModeKHR RendererVK::choose_swap_present_mode(const vector<VkPresentModeKHR>& p_available_present_modes) {
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
    int extent_width, extent_height;
    SDL_GetWindowSizeInPixels(window, &extent_width, &extent_height);
    VkExtent2D result;
    result.width = extent_width;
    result.height = extent_height;
    return result;
}

RendererVK::vector<Ref<RendererVK::PipelineInfo>> RendererVK::compile_pipeline(const std::string& p_pipeline_name, Ref<RendTaskInfo> p_root_task) {
    if (p_root_task == MEM_NULL) {
        throw std::runtime_error("Root task of a render pipeline cannot be NULL.");
    }
    if (p_root_task->prev.size() != 1 || p_root_task->prev[0] != RT_PRE_EXECUTE_PIPELINE_VK) {
        throw std::runtime_error("The first task of the render pipeline must have only RT_PREPARE_SWAP_CHAIN_IMAGE as it's previous task.");
    }
    vector<Ref<RendererVK::PipelineInfo>> pipeline_infos(allocator);

    pipeline_infos.reserve(frames_in_flight);
    for (size_t i = 0; i < frames_in_flight; ++i) {
        // Copy frames_in_flight amount of pipelines for each swap chain image to draw without waiting for the previous one to finish.
        auto pipeline_info = make_ref<PipelineInfo>(allocator, allocator);
        pipeline_info->pipeline_name = p_pipeline_name;
        compile_tasks(p_root_task, pipeline_info, i);
        pipeline_infos.push_back(pipeline_info);
    }
    return pipeline_infos;
}

void RendererVK::compile_tasks(Ref<RendTaskInfo> p_root_task, Ref<PipelineInfo> p_pipeline_info, uint32_t p_index) {
    // Using bfs, as the order where the render task will be pushed to the queue.
    std::map<RendTaskID, std::pair<Ref<RenderTaskVK>, std::vector<RendTaskID>*>> render_task_map;
    // Setup image available semaphores
    auto image_available_semaphore = make_ref<SemaphoreVK>(allocator, &base_render_data, true);
    p_pipeline_info->image_available_semaphore = image_available_semaphore;

    // Setup start task.
    Ref<RTPreExecutePipelineFactoryVK> start_task_factory
        = make_ref<RTPreExecutePipelineFactoryVK>(allocator, &base_render_data, image_available_semaphore);
    Ref<RenderTaskVK> start_task = start_task_factory->create().dynamic_cast_ref<RenderTaskVK>();
    start_task->task_semaphore = make_ref<SemaphoreVK>(allocator, &base_render_data, true);
    p_pipeline_info->task_infos.push_back({.task_id = RT_PRE_EXECUTE_PIPELINE_VK, .render_task = start_task});
    std::vector<RendTaskID> dummy;
    render_task_map.insert(std::pair(RT_PRE_EXECUTE_PIPELINE_VK, std::pair(start_task, &dummy)));

    compile_render_graph(p_root_task, p_pipeline_info, render_task_map);

    // Setup other properties according to the constructed graph.
    uint64_t max_depth = 0;
    for (auto rend_task : p_pipeline_info->task_infos) {
        auto* dep_tasks = render_task_map[rend_task.task_id].second;
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
                                      std::map<RendTaskID, std::pair<Ref<RenderTaskVK>, std::vector<RendTaskID>*>>& p_render_task_map) {
    // Process the tasks with bfs, which will also be the order of the tasks that will be sumitted to the queue.
    // Performing BFS should be the most optimal way for optimizing synchronozation, since the nodes with the same
    // depth has no dependencies among them.
    deque<Ref<RendTaskInfo>> to_visit(allocator);
    to_visit.push_back(p_root_task);
    hash_set<Ref<RendTaskInfo>> visited(allocator);
    while (!to_visit.empty()) {
        Ref<RendTaskInfo> curr = to_visit.front();
        to_visit.pop_front();
        if (visited.find(curr) != visited.end()) {
            continue;
        }
        visited.insert(curr);
        for (auto next : curr->next) {
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
        register_render_resources(curr, task_info, p_pipeline_info);
        // Push the node to the pipeline.
        p_pipeline_info->task_infos.push_back(task_info);

        // Keep track on the tasks' dependencies.
        p_render_task_map[task_info.task_id] = std::pair(task_info.render_task, &(curr->prev));
        // Initializes semaphore. If more than one task depend on this task, create a timeline semaphore. Otherwise use a binary semaphore.
        task_info.render_task->task_semaphore = make_ref<SemaphoreVK>(base_render_data.allocator, &base_render_data, curr->next.size() <= 1);
        if (curr->next.empty()) {
            // If this is the last task, keep track on their semaphores, to track when the tasks finish.
            p_pipeline_info->result_semaphores.push_back(task_info.render_task->task_semaphore);
        }
    }
}

Ref<RendResVK> RendererVK::get_rend_res(RendResID p_res, RendResTID p_type_id) {
    auto iter = render_resources.find(p_res);
    if (iter == render_resources.end() || !iter->second.is_valid()) {
        auto rend_res = make_ref<RendResVK>(allocator, &base_render_data);
        rend_res->res_type_id = p_type_id;
        rend_res->res_id = p_res;
        // TODO: initialize resource.
        rend_res->res = MEM_NULL;
        render_resources.insert({p_res, rend_res});
        return rend_res;
    }
    return iter->second.lock();
}

void RendererVK::register_render_resources(Ref<RendTaskInfo> p_task, TaskInfo& p_task_info, Ref<PipelineInfo> p_pipeline) {
    WBE_DEBUG_ASSERT(p_task != MEM_NULL);
    WBE_DEBUG_ASSERT(p_task_info.render_task != MEM_NULL);
    WBE_DEBUG_ASSERT(p_pipeline != MEM_NULL);
    for (auto& res : p_task->req_res) {
        Ref<RendResVK> rend_res = get_rend_res(res.res_id, res.type_id);
        p_pipeline->rend_res.push_back(rend_res);
        p_task_info.render_task->req_rend_res.push_back(rend_res);
    }
}

uint32_t RendererVK::find_memory_type(uint32_t p_type_filter, VkMemoryPropertyFlags p_properties) const {
    for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; ++i) {
        // Find the first memory property that could be used.
        if (p_type_filter & (1 >> i)) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find a suitable physical device.");
}
}
