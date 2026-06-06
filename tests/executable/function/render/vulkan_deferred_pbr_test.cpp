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
#include "core/reflection/reflection_defs.hh"
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
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
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
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <glm/ext/vector_float4.hpp>
#include <glm/matrix.hpp>
#include <memory>
#include <print>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace WhiteBirdEngine {
WBE_LABEL(REND_RES_TEST_DEFERRED_GBUFFER_FRAMEBUFFER, WBE_RENDER_RESOURCE);
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
        const std::set<uint32_t> queue_families(
            base_render_data->queue_family_indices.begin(), base_render_data->queue_family_indices.end());

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
                                        .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                        .set_queue_families(queue_families);
        auto normal_image_builder = WBE::ROImageVK::Builder()
                                        .set_image_type(VK_IMAGE_TYPE_2D)
                                        .set_format(GBUFFER_NORMAL_FORMAT)
                                        .image_2d(extent)
                                        .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                        .set_queue_families(queue_families);
        auto rma_image_builder = WBE::ROImageVK::Builder()
                                     .set_image_type(VK_IMAGE_TYPE_2D)
                                     .set_format(GBUFFER_RMA_FORMAT)
                                     .image_2d(extent)
                                     .set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                     .set_queue_families(queue_families);
        auto depth_image_builder = WBE::ROImageVK::Builder()
                                       .set_image_type(VK_IMAGE_TYPE_2D)
                                       .set_format(GBUFFER_DEPTH_FORMAT)
                                       .image_2d(extent)
                                       .set_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                       .set_queue_families(queue_families);

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
        // G-buffer render pass: 3 color attachments + 1 depth attachment.
        // Final layout for all attachments is SHADER_READ_ONLY_OPTIMAL so the
        // composition / albedo-debug pass can sample them.
        // -------------------------------------------------------------------
        auto make_color_attachment = [](VkFormat p_format) {
            VkAttachmentDescription attachment{};
            attachment.format = p_format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return attachment;
        };
        VkAttachmentDescription albedo_attachment = make_color_attachment(GBUFFER_ALBEDO_FORMAT);
        VkAttachmentDescription normal_attachment = make_color_attachment(GBUFFER_NORMAL_FORMAT);
        VkAttachmentDescription rma_attachment = make_color_attachment(GBUFFER_RMA_FORMAT);
        VkAttachmentDescription depth_attachment{};
        depth_attachment.format = GBUFFER_DEPTH_FORMAT;
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkAttachmentReference, 3> color_refs{
            VkAttachmentReference{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            VkAttachmentReference{.attachment = 1, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            VkAttachmentReference{.attachment = 2, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        };
        VkAttachmentReference depth_ref{
            .attachment = 3,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
        subpass.pColorAttachments = color_refs.data();
        subpass.pDepthStencilAttachment = &depth_ref;

        VkSubpassDependency dependency_in{};
        dependency_in.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency_in.dstSubpass = 0;
        dependency_in.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dependency_in.srcAccessMask = 0;
        dependency_in.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency_in.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkSubpassDependency dependency_out{};
        dependency_out.srcSubpass = 0;
        dependency_out.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency_out.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency_out.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency_out.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency_out.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        auto gbuffer_render_pass = WBE::RORenderPassVK::Builder()
                                       .add_attachment(albedo_attachment)
                                       .add_attachment(normal_attachment)
                                       .add_attachment(rma_attachment)
                                       .add_attachment(depth_attachment)
                                       .add_subpass(subpass)
                                       .add_dependency(dependency_in)
                                       .add_dependency(dependency_out)
                                       .create_ref(base_render_data);

        // -------------------------------------------------------------------
        // G-buffer framebuffer.
        // -------------------------------------------------------------------
        auto gbuffer_framebuffer = WBE::ROFramebufferVK::Builder()
                                       .add_attachment(gbuffer_albedo_view)
                                       .add_attachment(gbuffer_normal_view)
                                       .add_attachment(gbuffer_rma_view)
                                       .add_attachment(gbuffer_depth_view)
                                       .set_extent(extent)
                                       .set_render_pass(gbuffer_render_pass)
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
        //      cube into the manually-created G-buffer framebuffer.
        //   2) Albedo-debug pass: hand-written task that samples the G-buffer
        //      albedo attachment and writes it to the swap-chain framebuffer.
        // TODO: replace the albedo-debug pass with the real PBR composition
        //       pass once a multi-pass pipeline compiler is available.
        // -------------------------------------------------------------------
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = WBE::REND_RES_TEST_DEFERRED_GBUFFER_FRAMEBUFFER,
            .res_type_id = WBE::TypeUUID<WBE::ROFramebufferVK>::UUID,
            .res = gbuffer_framebuffer});

        // ---- (1) G-buffer pass: compiled from the resource system. -------
        auto gbuffer_pipeline_resource = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUGraphicsPipeline>("deferred_gbuffer");
        if (gbuffer_pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup deferred test: 'deferred_gbuffer' graphics pipeline resource not found.");
        }

        WBE::GraphicsPipelineCompilerVKContext compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = render_database_ref,
            .render_pass = gbuffer_render_pass,
            .renderer = renderer,
            .color_attachment_count = 3,
        };
        std::array<WBE::RenderUniformBinding, 2> gbuffer_uniform_bindings{
            WBE::RenderUniformBinding{.field_path = "camera_info.view", .data = &camera_view, .size = sizeof(camera_view)},
            WBE::RenderUniformBinding{.field_path = "camera_info.proj", .data = &camera_proj, .size = sizeof(camera_proj)},
        };
        std::array<VkClearValue, 4> gbuffer_clear_values{};
        gbuffer_clear_values[0].color = {.float32 = {0.0F, 0.0F, 0.0F, 0.0F}};
        gbuffer_clear_values[1].color = {.float32 = {0.0F, 0.0F, 0.0F, 0.0F}};
        gbuffer_clear_values[2].color = {.float32 = {0.0F, 0.0F, 0.0F, 0.0F}};
        gbuffer_clear_values[3].depthStencil = {.depth = 1.0F, .stencil = 0};

        auto [graphics_pipeline, descriptor_sets] =
            WBE::GraphicsPipelineCompilerVK(compiler_context)
                .compile("deferred_gbuffer",
                    gbuffer_pipeline_resource,
                    std::span<const WBE::RenderUniformBinding>(gbuffer_uniform_bindings.data(), gbuffer_uniform_bindings.size()));
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
            size_t global_alloc_max = WBE::EngineCore::get_singleton()->pool_allocator->get_internal_fragmentation_tracker();
            WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Global allocator max used: {}", global_alloc_max));
            WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
        }
    }
    const auto& global_profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
    for (const auto& profile : global_profiles) {
        WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds", profile.message, profile.delta));
    }
    return EXIT_SUCCESS;
}
