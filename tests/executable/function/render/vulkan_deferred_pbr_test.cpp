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
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <array>
#include <cstdlib>
#include <format>
#include <glm/ext/vector_float4.hpp>
#include <glm/matrix.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace WhiteBirdEngine {
inline constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW";
inline constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW";
inline constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW";
inline constexpr std::string_view REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW = "REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW";
// TODO
} // namespace WhiteBirdEngine

namespace {
constexpr VkFormat GBUFFER_ALBEDO_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
constexpr VkFormat GBUFFER_RMA_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
} // namespace

class ApplicationVulkanDeferredPBRTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double /* p_delta */) override {
    }

    virtual void render_update(double /* p_delta */) override {
        // The compiler-built G-buffer pipeline owns its own uniform buffers,
        // which are populated at compile-time from the caller-provided
        // uniform bindings. Per-frame updates (e.g. animated cameras) are not
        // wired up yet -- TODO: expose a hook on compiled pipelines to update
        // uniform values each frame and then animate the camera here.
    }

    void setup_graphics_pipeline() { // NOLINT
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        auto* render_database = WBE::RenderDatabase::get_singleton();
        auto render_database_ref = renderer->get_render_database();
        const auto* base_render_data = renderer->get_base_render_data();
        const VkExtent2D extent = base_render_data->swap_chain_extent;

        // Setup cube's mesh and inject its static-opaque buffers into the graph.
        auto cube_mesh = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUMesh3D>("cube");
        render_database->set_static_opaque_triangles(cube_mesh.get());
        renderer->add_job([renderer]() {
            renderer->update_static_opaque_triangles();
        });

        // -------------------------------------------------------------------
        // G-buffer images and image views.
        // -------------------------------------------------------------------
        auto albedo_image_builder = WBE::ROImageVK::Builder()
                                        .set_image_type(VK_IMAGE_TYPE_2D)
                                        .set_format(GBUFFER_ALBEDO_FORMAT)
                                        .image_2d(extent)
                                        .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        auto normal_image_builder = WBE::ROImageVK::Builder()
                                        .set_image_type(VK_IMAGE_TYPE_2D)
                                        .set_format(GBUFFER_NORMAL_FORMAT)
                                        .image_2d(extent)
                                        .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        auto rma_image_builder = WBE::ROImageVK::Builder()
                                     .set_image_type(VK_IMAGE_TYPE_2D)
                                     .set_format(GBUFFER_RMA_FORMAT)
                                     .image_2d(extent)
                                     .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        auto depth_image_builder = WBE::ROImageVK::Builder()
                                       .set_image_type(VK_IMAGE_TYPE_2D)
                                       .set_format(GBUFFER_DEPTH_FORMAT)
                                       .image_2d(extent)
                                       .set_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        gbuffer_albedo_image = albedo_image_builder.create_ref(base_render_data);
        gbuffer_normal_image = normal_image_builder.create_ref(base_render_data);
        gbuffer_rma_image = rma_image_builder.create_ref(base_render_data);
        gbuffer_depth_image = depth_image_builder.create_ref(base_render_data);

        VkImageSubresourceRange color_subresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        VkImageSubresourceRange depth_subresource{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        gbuffer_albedo_view = WBE::ROImageViewVK::Builder()
                                  .set_image(gbuffer_albedo_image)
                                  .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                  .set_format(GBUFFER_ALBEDO_FORMAT)
                                  .set_subresource_range(color_subresource)
                                  .create_ref(base_render_data);
        gbuffer_normal_view = WBE::ROImageViewVK::Builder()
                                  .set_image(gbuffer_normal_image)
                                  .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                  .set_format(GBUFFER_NORMAL_FORMAT)
                                  .set_subresource_range(color_subresource)
                                  .create_ref(base_render_data);
        gbuffer_rma_view = WBE::ROImageViewVK::Builder()
                               .set_image(gbuffer_rma_image)
                               .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                               .set_format(GBUFFER_RMA_FORMAT)
                               .set_subresource_range(color_subresource)
                               .create_ref(base_render_data);
        gbuffer_depth_view = WBE::ROImageViewVK::Builder()
                                 .set_image(gbuffer_depth_image)
                                 .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
                                 .set_format(GBUFFER_DEPTH_FORMAT)
                                 .set_subresource_range(depth_subresource)
                                 .create_ref(base_render_data);

        // -------------------------------------------------------------------
        // Camera setup. Static for now (see render_update TODO).
        // -------------------------------------------------------------------
        const float aspect_ratio = base_render_data->swap_chain_aspect_ratio;
        glm::mat4 camera_view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F));
        glm::mat4 camera_proj = glm::perspective(glm::radians(45.0F), aspect_ratio, 0.1F, 10.0F);
        camera_proj[1][1] *= -1;

        // -------------------------------------------------------------------
        // Compose the render graph descriptor.
        //   1) G-buffer pass: compiled from the RUGraphicsPipeline resource
        //      "deferred_gbuffer" using the pipeline compiler. Renders the
        //      cube into the G-buffer image-view attachments.
        //   2) Albedo-debug pass: hand-written task that samples the G-buffer
        //      albedo attachment and writes it to the swap-chain image view.
        // TODO: replace the albedo-debug pass with the real PBR composition
        //       pass once a multi-pass pipeline compiler is available.
        // -------------------------------------------------------------------
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_DEFERRED_GBUFFER_ALBEDO_IMAGE_VIEW),
            .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
            .res = gbuffer_albedo_view});
        pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_DEFERRED_GBUFFER_NORMAL_IMAGE_VIEW),
            .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
            .res = gbuffer_normal_view});
        pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_DEFERRED_GBUFFER_RMA_IMAGE_VIEW),
            .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
            .res = gbuffer_rma_view});
        pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = std::string(WBE::REND_RES_TEST_DEFERRED_GBUFFER_DEPTH_IMAGE_VIEW),
            .res_type_id = WBE::TypeUUID<WBE::ROImageViewVK>::UUID,
            .res = gbuffer_depth_view});

        // ---- (1) G-buffer pass: compiled from the resource system. -------
        auto gbuffer_pipeline_resource = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUGraphicsPipeline>("deferred_gbuffer");
        if (gbuffer_pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup deferred test: 'deferred_gbuffer' graphics pipeline resource not found.");
        }

        WBE::GraphicsPipelineCompilerVKContext compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = render_database_ref,
            .renderer = renderer,
            .color_attachment_formats = {GBUFFER_ALBEDO_FORMAT, GBUFFER_NORMAL_FORMAT, GBUFFER_RMA_FORMAT},
            .depth_attachment_format = GBUFFER_DEPTH_FORMAT,
        };
        WBE::RenderBindpointDeclarationsBuilder gbuffer_bindpoint_builder;
        gbuffer_bindpoint_builder.add_buffer_decleration("camera_info.view", &camera_view, sizeof(camera_view))
            .add_buffer_decleration("camera_info.proj", &camera_proj, sizeof(camera_proj));
        auto gbuffer_bindpoint_declarations = gbuffer_bindpoint_builder.build();
        std::array<VkClearValue, 4> gbuffer_clear_values{};
        gbuffer_clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 0.0F}};
        gbuffer_clear_values[1].color = {.float32 = {0.0F, 0.0F, 0.0F, 0.0F}};
        gbuffer_clear_values[2].color = {.float32 = {0.0F, 0.0F, 0.0F, 0.0F}};
        gbuffer_clear_values[3].depthStencil = {.depth = 1.0F, .stencil = 0};

        auto compiled = WBE::GraphicsPipelineCompilerVK(compiler_context)
                            .compile("deferred_gbuffer", gbuffer_pipeline_resource, gbuffer_bindpoint_declarations);
        auto graphics_pipeline = compiled.graphics_pipeline;
        // TODO: Register render task with graphics pipeline.
        // ---- (2) Albedo-debug pass: hand-written. -------------------------
        // TODO

        renderer->register_render_graph("test_render_graph", std::move(pipeline_desc));
        renderer->activate_render_graph("test_render_graph");
    }

private:
    WBE::Ref<WBE::ROImageVK> gbuffer_albedo_image;
    WBE::Ref<WBE::ROImageVK> gbuffer_normal_image;
    WBE::Ref<WBE::ROImageVK> gbuffer_rma_image;
    WBE::Ref<WBE::ROImageVK> gbuffer_depth_image;
    WBE::Ref<WBE::ROImageViewVK> gbuffer_albedo_view;
    WBE::Ref<WBE::ROImageViewVK> gbuffer_normal_view;
    WBE::Ref<WBE::ROImageViewVK> gbuffer_rma_view;
    WBE::Ref<WBE::ROImageViewVK> gbuffer_depth_view;
    WBE::Ref<WBE::ROSamplerVK> gbuffer_sampler;
};

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    {
        ApplicationVulkanDeferredPBRTest game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
            game_application.setup_graphics_pipeline();
            game_application.run();
            size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_internal_fragmentation_tracker();
            auto global_alloc_max_by_arena = WBE::EngineCore::get_singleton()->pool_allocator->get_internal_fragmentation_tracker();
            for (size_t arena_index = 0; arena_index < global_alloc_max_by_arena.size(); ++arena_index) {
                WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)
                    ->message(std::format("Global allocator arena {} max used: {}", arena_index, global_alloc_max_by_arena[arena_index]));
            }
            WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
        }
    }
    const auto& global_profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
    for (const auto& profile : global_profiles) {
        WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds", profile.message, profile.delta));
    }
    return EXIT_SUCCESS;
}
