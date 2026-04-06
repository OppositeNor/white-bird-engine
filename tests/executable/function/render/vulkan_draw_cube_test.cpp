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
#include "core/reflection/reflection_defs.hh"
#include "function/render/render_database.hh"
#include "function/function_utils.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_framebuffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/trigonometric.hpp"
#include "generated/type_uuid.gen.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/render/render_task.hh"
#include "function/render/i_renderer.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "global/global.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdlib>
#include <exception>
#include <format>
#include <memory>
#include <print>
#include <glm/ext/matrix_transform.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace WhiteBirdEngine {
WBE_LABEL(REND_RES_TEST_DRAW_CUBUE_GRAPHICS_PIPELINE, WBE_RENDER_RESOURCE);
}

class ApplicationVulkanDrawPipelineTest : public WBE::ApplicationGame {
public:

    virtual void logic_update(double p_delta) override {
        // TODO
    }

    virtual void render_update(double p_delta) override {
        camera_info.view = glm::rotate(camera_info.view, static_cast<float>(p_delta), glm::vec3(0.0F, 1.0F, 0.0F));
        camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
    }

    void setup_graphics_pipeline() {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        auto* render_database = WBE::RenderDatabase::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();

        // Setup cube's mesh
        auto cube_mesh = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUMesh3D>("cube");
        render_database->set_static_opaque_triangles(cube_mesh.get());
        renderer->add_job([renderer]() {
            renderer->update_static_opaque_triangles();
        });

        WBE::ROGraphicsPipelineVK::Config::ShaderConfig shader_config;
        // Setup shaders
        shader_config.set_shaders(base_render_data, "draw_cube_vert", "draw_cube_frag");

        // Setup graphics pipeline layout.
        auto descriptor_set_layout = WBE::make_ref<WBE::RODescriptorSetLayoutVK>(render_allocator, base_render_data,
                                                                                 WBE::RODescriptorSetLayoutVK::Builder()
                                                                                 .add_ubo(0, 1));
        auto graphics_pipeline_layout = WBE::make_ref<WBE::ROPipelineLayoutVK>(render_allocator, base_render_data,
                                                                               WBE::ROPipelineLayoutVK::Builder()
                                                                               .add_descriptor_set_layout(descriptor_set_layout));
        auto render_pass = renderer->get_swap_chain_render_pass();
        auto graphics_pipeline = WBE::ROGraphicsPipelineVK::Builder()
            .default_render_opaque_configuration(
                base_render_data, shader_config, graphics_pipeline_layout, render_pass)
            .create_ref(base_render_data);
        aspect_ratio = base_render_data->swap_chain_aspect_ratio;
        camera_info = WBE::CameraInfoVKUBO {
            // Look at the origin from (2,2,2)
            .view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F),
                                glm::vec3(0.0F, 0.0F, 0.0F),
                                glm::vec3(0.0F, 1.0F, 0.0F)),
            .proj = glm::perspective(glm::radians(45.0F),
                                     aspect_ratio,
                                     0.1F, 10.0F),
        };
        // Inverse the y value since Vulkan makes y axis points downwards.
        camera_info.proj[1][1] *= -1;
        // Setup time UBO
        camera_info_ubo = WBE::make_ref<WBE::ROBufferVK>(render_allocator, base_render_data,
                                                         WBE::ROBufferVK::Builder()
                                                         .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                                         .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                                                         .set_size(sizeof(WBE::CameraInfoVKUBO)));
        camera_info_ubo->update_buffer(&camera_info, sizeof(WBE::CameraInfoVKUBO));
        // Setup descriptor sets.
        auto descriptor_set = WBE::make_ref<WBE::RODescriptorSetVK>(render_allocator, base_render_data,
                                                                    WBE::RODescriptorSetVK::Builder()
                                                                    .add_descriptor_set_layout(descriptor_set_layout));
        descriptor_set->write_descriptor_set(
            WBE::RODescriptorSetVK::Writer()
            .set_binding(0)
            .add_uniform_buffer_info(
                WBE::RODescriptorSetVK::Writer::BufferInfo{
                    .buffer = camera_info_ubo,
                    .offset = 0
                })
        );
        // Setup render task
        std::vector<WBE::Ref<WBE::RODescriptorSetVK>> descriptor_sets {descriptor_set};
        auto rend_task_info = WBE::RendTaskInfo::Builder()
            .set_task_id(WBE::RT_RUN_PIPELINE)
            .add_req_res(WBE::REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK, WBE::TypeUUID<WBE::ROFramebufferVK>::UUID)
            .add_req_res(WBE::REND_RES_TEST_DRAW_CUBUE_GRAPHICS_PIPELINE, WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID)
            .add_req_res(WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK, WBE::TypeUUID<WBE::ROBufferVK>::UUID)
            .add_req_res(WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK, WBE::TypeUUID<WBE::ROBufferVK>::UUID)
            .set_render_task_factory(WBE::make_ref<WBE::RTRunGraphicsPipelineFactoryVK>(
                render_allocator, renderer->get_base_render_data(), descriptor_sets, true))
            .build(render_allocator);
        WBE::RendPipelineInfo pipeline_info;
        pipeline_info
            .set_root_task(rend_task_info)
            .add_resource(WBE::REND_RES_TEST_DRAW_CUBUE_GRAPHICS_PIPELINE,
                          WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
                          graphics_pipeline);
        get_renderer()->register_pipeline("test_pipeline", pipeline_info);
        get_renderer()->activate_pipeline("test_pipeline");
    }

private:
    WBE::Ref<WBE::ROBufferVK> camera_info_ubo;
    WBE::CameraInfoVKUBO camera_info{};
    // double time_accumulator = 0.0;
    float aspect_ratio = 1.0F;
};

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    try {
        {
            ApplicationVulkanDrawPipelineTest game_application;
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
            WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds",
                                                                          profile.message, profile.delta));
        }
    } catch (const std::exception& e) {
        std::println("Exception caught in main: {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

