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
#include "function/function_utils.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdlib>
#include <exception>
#include <format>
#include <glm/ext/vector_float4.hpp>
#include <memory>
#include <print>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

namespace WhiteBirdEngine {
WBE_LABEL(REND_RES_TEST_DRAW_TRIANGLE_GRAPHICS_PIPELINE, WBE_RENDER_RESOURCE);
}

class ApplicationVulkanDrawPipelineTest : public WBE::ApplicationGame {
public:
    virtual void logic_update(double p_delta) override {
    }

    virtual void render_update(double p_delta) override {
        time_accumulator += p_delta;
        WBE::TimeVKUBO time_ubo_data{.duration_sec = static_cast<float>(time_accumulator)};
        time_ubo->update_buffer(&time_ubo_data, sizeof(WBE::TimeVKUBO));
    }

    void setup_graphics_pipeline() {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();

        WBE::ROGraphicsPipelineVK::Config::ShaderConfig shader_config;
        // Setup shaders
        // Setup shaders
        auto vertex_shader = WBE::ROShaderVK::Builder().set_code("draw_cube_unshaded_vert").create_ref(base_render_data);
        auto fragment_shader = WBE::ROShaderVK::Builder().set_code("draw_cube_unshaded_frag").create_ref(base_render_data);
        shader_config.set_shaders(vertex_shader, fragment_shader);

        // Setup graphics pipeline layout.
        WBE::Ref<WBE::RODescriptorSetLayoutVK> descriptor_set_layout = WBE::make_ref<WBE::RODescriptorSetLayoutVK>(
            render_allocator, base_render_data, WBE::RODescriptorSetLayoutVK::Builder().add_ubo(0, 1));
        WBE::Ref<WBE::ROPipelineLayoutVK> graphics_pipeline_layout = WBE::make_ref<WBE::ROPipelineLayoutVK>(
            render_allocator, base_render_data, WBE::ROPipelineLayoutVK::Builder().add_descriptor_set_layout(descriptor_set_layout));
        auto graphics_pipeline = WBE::ROGraphicsPipelineVK::Builder()
                                     .default_render_opaque_configuration(base_render_data, shader_config, graphics_pipeline_layout)
                                     .create_ref(base_render_data);
        WBE::TimeVKUBO time_ubo_data{.duration_sec = 0.5F};
        // Setup time UBO
        time_ubo = WBE::make_ref<WBE::ROBufferVK>(render_allocator,
            base_render_data,
            WBE::ROBufferVK::Builder()
                .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                .set_size(sizeof(WBE::TimeVKUBO)));
        time_ubo->update_buffer(&time_ubo_data, sizeof(WBE::TimeVKUBO));
        // Setup descriptor sets.
        WBE::Ref<WBE::RODescriptorSetVK> descriptor_set = WBE::make_ref<WBE::RODescriptorSetVK>(
            render_allocator, base_render_data, WBE::RODescriptorSetVK::Builder().add_descriptor_set_layout(descriptor_set_layout));
        descriptor_set->write_descriptor_set(WBE::RODescriptorSetVK::Writer().set_binding(0).add_uniform_buffer_info(
            WBE::RODescriptorSetVK::Writer::BufferInfo{.buffer = time_ubo, .offset = 0}));
        // Setup render task. The vertex/index data lives in the global static
        // opaque mesh, populated by RendererVK::update_static_opaque_triangles()
        // from the RenderDatabase, and exposed to render tasks under the
        // REND_RES_STATIC_OPAQUE_TRIANGLES_{VERTICES,INDICES}_VK global IDs.
        std::vector<WBE::Ref<WBE::RODescriptorSetVK>> descriptor_sets{descriptor_set};
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.resources.emplace_back(WBE::RendResVK{.res_id = WBE::REND_RES_TEST_DRAW_TRIANGLE_GRAPHICS_PIPELINE,
            .res_type_id = WBE::TypeUUID<WBE::ROGraphicsPipelineVK>::UUID,
            .res = graphics_pipeline});
        WBE::RendTaskDescVK task_desc(render_allocator);
        task_desc.task_id = WBE::RT_RUN_PIPELINE;
        task_desc.create = [render_allocator, base_render_data, descriptor_sets]() -> WBE::Unique<WBE::RenderTaskVK> {
            return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(render_allocator,
                base_render_data,
                glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
                base_render_data->swap_chain_extent,
                WBE::REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK,
                WBE::REND_RES_TEST_DRAW_TRIANGLE_GRAPHICS_PIPELINE,
                WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
                WBE::REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
                std::span<const WBE::Ref<WBE::RODescriptorSetVK>>(descriptor_sets.data(), descriptor_sets.size()));
        };
        pipeline_desc.tasks.emplace_back(std::move(task_desc));
        WBE::RendererVK::get_singleton()->register_render_graph("test_render_graph", std::move(pipeline_desc));
        WBE::RendererVK::get_singleton()->activate_render_graph("test_render_graph");
    }

private:
    WBE::Ref<WBE::ROBufferVK> time_ubo;
    double time_accumulator = 0.0;
};

int main(int p_argc, char* p_argv[]) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Directory({"test_env"}));
    {
        ApplicationVulkanDrawPipelineTest game_application;
        // Setup triangle's mesh
        auto triangle_mesh = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUMesh3D>("triangle");
        WBE::RenderDatabase::get_singleton()->set_static_opaque_triangles(triangle_mesh.get());
        game_application.get_renderer()->update_static_opaque_triangles();
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
