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
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/profiling/profiler.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/spot_light_info.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <memory>
#include <print>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace {

constexpr VkFormat SHADOWMAP_DEPTH_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_DEPTH_FORMAT;
constexpr VkFormat SHADOWMAP_COLOR_FORMAT = WBE::ROImageVK::Builder::SHADOW_MAP_COLOR_FORMAT;

WBE::CameraInfoVKUBO default_camera_info(float p_aspect_ratio) {
    WBE::CameraInfoVKUBO camera_info{
        .view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F)),
        .proj = glm::perspective(glm::radians(45.0F), p_aspect_ratio, 0.1F, 10.0F),
    };
    camera_info.proj[1][1] *= -1;
    return camera_info;
}

} // namespace

class ApplicationVulkanDrawCubeTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double /* p_delta */) override {
    }

    virtual void render_update(double p_delta) override {
        if (camera_info_ubo == WBE::MEM_NULL) {
            return;
        }
        camera_info.view = glm::rotate(camera_info.view, static_cast<float>(p_delta), glm::vec3(0.0F, 1.0F, 0.0F));
        camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    }

    void setup_draw_task() { // NOLINT(readability-function-cognitive-complexity)
        auto* resource_manager = WBE::ResourceManager::get_singleton();
        auto cube_mesh = resource_manager->get_resource<WBE::RUMesh3D>("cube");
        auto default_material = resource_manager->get_resource<WBE::RUMaterial>("default_material");
        auto shadow_pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("shadowmap_spot");
        auto* renderer_vk = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer_vk->get_base_render_data();
        if (cube_mesh == nullptr) {
            throw std::runtime_error("Failed to setup draw cube test: cube mesh resource was not found.");
        }
        if (default_material == nullptr || default_material->graphics_pipeline == nullptr ||
            default_material->graphics_pipeline->resource_key == nullptr) {
            throw std::runtime_error("Failed to setup draw cube test: default material has no compiled graphics pipeline key.");
        }
        if (shadow_pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup draw cube test: 'shadowmap_spot' graphics pipeline resource not found.");
        }

        auto* render_allocator = get_renderer_allocator().get();
        auto* render_database = WBE::RenderDatabase::get_singleton();
        render_database->set_static_opaque_triangles(cube_mesh.get());
        renderer_vk->update_static_opaque_triangles();

        // ----- Spot light setup ---------------------------------------------
        // Hard-coded for this test; in a real scene these would come from the
        // ECS scene snapshot. The render database converts the ECS components
        // into render-side `SpotLightInfo` records that include the
        // precomputed light-space view/proj matrices.
        std::array<WBE::ComponentSpotLight, 1> source_spot_lights{
            WBE::ComponentSpotLight{
                .position = glm::vec3(2.5F, 3.5F, 2.5F),
                .direction = glm::normalize(glm::vec3(-1.0F, -1.4F, -1.0F)),
                .color = glm::vec3(1.0F, 0.95F, 0.85F),
                .intensity = 8.0F,
                .inner_cone_cos = std::cos(glm::radians(18.0F)),
                .outer_cone_cos = std::cos(glm::radians(28.0F)),
                .range = 12.0F,
            },
        };
        render_database->set_static_spot_lights(
            std::span<const WBE::ComponentSpotLight>(source_spot_lights.data(), source_spot_lights.size()));
        const auto& static_spot_lights = render_database->get_static_spot_lights();
        if (static_spot_lights.empty()) {
            throw std::runtime_error("Failed to setup draw cube test: no spot lights registered.");
        }
        const WBE::SpotLightInfo& spot_light = static_spot_lights[0];

        // ----- Shadow map images ---------------------------------------------
        const VkExtent2D shadow_extent{
            .width = spot_light.shadow_map_resolution, .height = spot_light.shadow_map_resolution};
        const std::set<uint32_t> queue_families(
            base_render_data->queue_family_indices.begin(), base_render_data->queue_family_indices.end());

        shadow_depth_image = WBE::ROImageVK::Builder()
                                 .shadow_map_depth(spot_light)
                                 .set_queue_families(queue_families)
                                 .create_ref(base_render_data);
        shadow_color_image = WBE::ROImageVK::Builder()
                                 .shadow_map_color(spot_light)
                                 .set_queue_families(queue_families)
                                 .create_ref(base_render_data);

        VkImageSubresourceRange depth_subresource{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        VkImageSubresourceRange color_subresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        shadow_depth_view = WBE::ROImageViewVK::Builder()
                                .set_image(shadow_depth_image)
                                .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                .set_format(SHADOWMAP_DEPTH_FORMAT)
                                .set_subresource_range(depth_subresource)
                                .create_ref(base_render_data);
        shadow_color_view = WBE::ROImageViewVK::Builder()
                                .set_image(shadow_color_image)
                                .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                .set_format(SHADOWMAP_COLOR_FORMAT)
                                .set_subresource_range(color_subresource)
                                .create_ref(base_render_data);

        shadow_sampler = WBE::ROSamplerVK::Builder()
                             .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                             .set_address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                             .create_ref(base_render_data);

        // ----- Shadow map render pass / framebuffer --------------------------
        VkAttachmentDescription shadow_color_attachment{};
        shadow_color_attachment.format = SHADOWMAP_COLOR_FORMAT;
        shadow_color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        shadow_color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        shadow_color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        shadow_color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        shadow_color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        shadow_color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        shadow_color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription shadow_depth_attachment{};
        shadow_depth_attachment.format = SHADOWMAP_DEPTH_FORMAT;
        shadow_depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        shadow_depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        shadow_depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        shadow_depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        shadow_depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        shadow_depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        shadow_depth_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference shadow_color_ref{
            .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference shadow_depth_ref{
            .attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription shadow_subpass{};
        shadow_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        shadow_subpass.colorAttachmentCount = 1;
        shadow_subpass.pColorAttachments = &shadow_color_ref;
        shadow_subpass.pDepthStencilAttachment = &shadow_depth_ref;

        VkSubpassDependency shadow_dependency_in{};
        shadow_dependency_in.srcSubpass = VK_SUBPASS_EXTERNAL;
        shadow_dependency_in.dstSubpass = 0;
        shadow_dependency_in.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        shadow_dependency_in.srcAccessMask = 0;
        shadow_dependency_in.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        shadow_dependency_in.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkSubpassDependency shadow_dependency_out{};
        shadow_dependency_out.srcSubpass = 0;
        shadow_dependency_out.dstSubpass = VK_SUBPASS_EXTERNAL;
        shadow_dependency_out.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        shadow_dependency_out.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        shadow_dependency_out.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        shadow_dependency_out.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        auto shadow_render_pass = WBE::RORenderPassVK::Builder()
                                      .add_attachment(shadow_color_attachment)
                                      .add_attachment(shadow_depth_attachment)
                                      .add_subpass(shadow_subpass)
                                      .add_dependency(shadow_dependency_in)
                                      .add_dependency(shadow_dependency_out)
                                      .create_ref(base_render_data);

        auto shadow_framebuffer = WBE::ROFramebufferVK::Builder()
                                      .add_attachment(shadow_color_view)
                                      .add_attachment(shadow_depth_view)
                                      .set_extent(shadow_extent)
                                      .set_render_pass(shadow_render_pass)
                                      .create_ref(base_render_data);

        // ----- Compile shadow pipeline ---------------------------------------
        WBE::GraphicsPipelineCompilerVKContext shadow_compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = renderer_vk->get_render_database(),
            .render_pass = shadow_render_pass,
            .renderer = renderer_vk,
            .color_attachment_count = 1,
        };
        std::array<WBE::RenderUniformBinding, 2> shadow_uniform_bindings{
            WBE::RenderUniformBinding{
                .field_path = "light_camera_info.view", .data = &spot_light.view, .size = sizeof(spot_light.view)},
            WBE::RenderUniformBinding{
                .field_path = "light_camera_info.proj", .data = &spot_light.proj, .size = sizeof(spot_light.proj)},
        };
        auto [shadow_graphics_pipeline, shadow_descriptor_sets] =
            WBE::GraphicsPipelineCompilerVK(shadow_compiler_context)
                .compile("shadowmap_spot",
                    shadow_pipeline_resource,
                    std::span<const WBE::RenderUniformBinding>(
                        shadow_uniform_bindings.data(), shadow_uniform_bindings.size()));

        // ----- Compile main forward-PBR pipeline -----------------------------
        camera_info = default_camera_info(base_render_data->swap_chain_aspect_ratio);
        std::array<WBE::RenderUniformBinding, 11> main_uniform_bindings{
            WBE::RenderUniformBinding{
                .field_path = "camera_info.view", .data = &camera_info.view, .size = sizeof(camera_info.view)},
            WBE::RenderUniformBinding{
                .field_path = "camera_info.proj", .data = &camera_info.proj, .size = sizeof(camera_info.proj)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.view", .data = &spot_light.view, .size = sizeof(spot_light.view)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.proj", .data = &spot_light.proj, .size = sizeof(spot_light.proj)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.position",
                .data = &spot_light.position,
                .size = sizeof(spot_light.position)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.intensity",
                .data = &spot_light.intensity,
                .size = sizeof(spot_light.intensity)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.direction",
                .data = &spot_light.direction,
                .size = sizeof(spot_light.direction)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.inner_cone_cos",
                .data = &spot_light.inner_cone_cos,
                .size = sizeof(spot_light.inner_cone_cos)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.color", .data = &spot_light.color, .size = sizeof(spot_light.color)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.outer_cone_cos",
                .data = &spot_light.outer_cone_cos,
                .size = sizeof(spot_light.outer_cone_cos)},
            WBE::RenderUniformBinding{
                .field_path = "spot_light_info.range", .data = &spot_light.range, .size = sizeof(spot_light.range)},
        };

        WBE::GraphicsPipelineCompilerVKContext main_compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = renderer_vk->get_render_database(),
            .render_pass = renderer_vk->get_swap_chain_render_pass(),
            .renderer = renderer_vk,
            .color_attachment_count = 1,
        };
        auto [main_graphics_pipeline, main_descriptor_sets] =
            WBE::GraphicsPipelineCompilerVK(main_compiler_context)
                .compile(default_material->graphics_pipeline->resource_key,
                    default_material->graphics_pipeline,
                    std::span<const WBE::RenderUniformBinding>(
                        main_uniform_bindings.data(), main_uniform_bindings.size()));

        // The compiler binds the placeholder shadowmap texture from the
        // material; overwrite it with the actual depth image rendered above.
        if (main_descriptor_sets.empty()) {
            throw std::runtime_error("Failed to setup draw cube test: main pipeline produced no descriptor sets.");
        }
        // Rebind binding 0 (camera_info) to a UBO we own so we can rotate the
        // camera each frame from render_update().
        camera_info_ubo = WBE::ROBufferVK::Builder()
                              .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                              .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                              .set_size(sizeof(WBE::CameraInfoVKUBO))
                              .create_ref(base_render_data);
        camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
        main_descriptor_sets[0]->write_descriptor_set(
            WBE::RODescriptorSetVK::Writer().set_binding(0).add_uniform_buffer_info(
                WBE::RODescriptorSetVK::Writer::BufferInfo{
                    .buffer = camera_info_ubo,
                    .offset = 0,
                    .range = sizeof(WBE::CameraInfoVKUBO),
                }));
        constexpr uint32_t SHADOWMAP_BINDING = 6;
        constexpr uint32_t SHADOWMAP_SAMPLER_BINDING = 7;
        main_descriptor_sets[0]->write_descriptor_set(
            WBE::RODescriptorSetVK::Writer().set_binding(SHADOWMAP_BINDING).add_sampled_image_info(
                WBE::RODescriptorSetVK::Writer::ImageInfo{
                    .image_view = shadow_depth_view,
                    .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                }));
        main_descriptor_sets[0]->write_descriptor_set(
            WBE::RODescriptorSetVK::Writer().set_binding(SHADOWMAP_SAMPLER_BINDING).add_sampler_info(
                WBE::RODescriptorSetVK::Writer::ImageInfo{
                    .sampler = shadow_sampler,
                }));

        // ----- Build render graph: shadow pass -> main pass ------------------
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.resources.emplace_back(
            WBE::RendResVK{.res_id = WBE::REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE,
                .res_type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
                .res = shadow_graphics_pipeline});
        pipeline_desc.resources.emplace_back(
            WBE::RendResVK{.res_id = WBE::REND_RES_TEST_SHADOWMAP_FRAMEBUFFER,
                .res_type_id = WBE::TypeUUID<WBE::ROFramebufferVK>::UUID,
                .res = shadow_framebuffer});
        pipeline_desc.resources.emplace_back(
            WBE::RendResVK{.res_id = WBE::REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE,
                .res_type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
                .res = main_graphics_pipeline});

        WBE::RendTaskDescVK shadow_task_desc(render_allocator);
        shadow_task_desc.task_id = WBE::RT_RUN_PIPELINE_SHADOWMAP;
        shadow_task_desc.create =
            [render_allocator, base_render_data, shadow_descriptor_sets]() -> WBE::Unique<WBE::RenderTaskVK> {
            std::array<VkClearValue, 2> shadow_clear_values{};
            shadow_clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}};
            shadow_clear_values[1].depthStencil = {.depth = 1.0F, .stencil = 0};
            return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(render_allocator,
                base_render_data,
                std::span<const VkClearValue>(shadow_clear_values.data(), shadow_clear_values.size()),
                WBE::REND_RES_TEST_SHADOWMAP_FRAMEBUFFER,
                WBE::REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE,
                WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
                WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
                std::span<const WBE::Ref<WBE::RODescriptorSetVK>>(
                    shadow_descriptor_sets.data(), shadow_descriptor_sets.size()));
        };

        WBE::RendTaskDescVK main_task_desc(render_allocator);
        main_task_desc.task_id = WBE::RT_RUN_PIPELINE;
        main_task_desc.prev.emplace_back(WBE::RT_RUN_PIPELINE_SHADOWMAP);
        main_task_desc.create =
            [render_allocator, base_render_data, main_descriptor_sets]() -> WBE::Unique<WBE::RenderTaskVK> {
            return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(render_allocator,
                base_render_data,
                glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
                WBE::REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK,
                WBE::REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE,
                WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
                WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
                std::span<const WBE::Ref<WBE::RODescriptorSetVK>>(
                    main_descriptor_sets.data(), main_descriptor_sets.size()));
        };

        pipeline_desc.tasks.emplace_back(std::move(shadow_task_desc));
        pipeline_desc.tasks.emplace_back(std::move(main_task_desc));
        renderer_vk->register_render_graph("draw_cube_pipeline", std::move(pipeline_desc));
        renderer_vk->activate_render_graph("draw_cube_pipeline");
    }

private:
    // Kept alive for the lifetime of the test because the framebuffer and
    // the main pipeline's descriptor set reference these by raw
    // VkImage/VkImageView/VkSampler handles without retaining ownership.
    WBE::Ref<WBE::ROImageVK> shadow_depth_image;
    WBE::Ref<WBE::ROImageVK> shadow_color_image;
    WBE::Ref<WBE::ROImageViewVK> shadow_depth_view;
    WBE::Ref<WBE::ROImageViewVK> shadow_color_view;
    WBE::Ref<WBE::ROSamplerVK> shadow_sampler;
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::CameraInfoVKUBO camera_info{};
};

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    try {
        {
            ApplicationVulkanDrawCubeTest game_application;
            {
                WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
                game_application.setup_draw_task();
                game_application.run();
                size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_internal_fragmentation_tracker();
                size_t global_alloc_max = WBE::EngineCore::get_singleton()->pool_allocator->get_internal_fragmentation_tracker();
                WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Global allocator max used: {}", global_alloc_max));
                WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
            }
        }
        const auto& global_profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
        for (const auto& profile : global_profiles) {
            WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds", profile.message, profile.delta));
        }
    } catch (const std::exception& e) {
        std::println("Exception caught in main: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
