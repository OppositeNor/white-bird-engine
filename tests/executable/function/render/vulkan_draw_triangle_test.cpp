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
#include "function/application/application_game.hh"
#include "function/function_utils.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include <cstdlib>
#include <format>
#include <glm/ext/vector_float4.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

inline constexpr std::string_view REND_RES_TEST_DRAW_TRIANGLE_GRAPHICS_PIPELINE = "draw_triangle_pipeline";

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

        WBE::TimeVKUBO time_ubo_data{.duration_sec = 0.5F};
        auto pipeline_resource = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUGraphicsPipeline>("draw_triangle_pipeline");
        if (pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup draw triangle test: graphics pipeline resource was not found.");
        }
        WBE::RenderBindpointDeclarationsBuilder bindpoint_builder;
        bindpoint_builder.add_buffer_decleration("time", &time_ubo_data, sizeof(time_ubo_data));
        WBE::GraphicsPipelineCompilerVKContext compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = renderer->get_render_database(),
            .renderer = renderer,
            .color_attachment_formats = {base_render_data->swap_chain_format},
        };
        auto compiled = WBE::GraphicsPipelineCompilerVK(compiler_context)
                            .compile("draw_triangle_pipeline", pipeline_resource, bindpoint_builder.build());
        auto graphics_pipeline = compiled.graphics_pipeline;
        // Setup time uniform buffer
        time_ubo = WBE::make_ref<WBE::ROBufferVK>(render_allocator,
            base_render_data,
            WBE::ROBufferVK::Builder()
                .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                .set_domain(WBE::DeviceDomain::HOST_DEVICE)
                .set_size(sizeof(WBE::TimeVKUBO)));
        time_ubo->update_buffer(&time_ubo_data, sizeof(WBE::TimeVKUBO));
        auto [set_index, binding] = compiled.descriptor_set_bindings.at("time");
        graphics_pipeline->write_descriptor_set(set_index,
            WBE::RODescriptorSetVK::Writer().set_binding(binding).add_uniform_buffer_info(
                WBE::RODescriptorSetVK::Writer::BufferInfo{.buffer = time_ubo, .offset = 0, .range = sizeof(WBE::TimeVKUBO)}));
        // Setup render task. The vertex/index data lives in the global static
        // opaque mesh, populated by RendererVK::update_static_triangle_set()
        // from the RenderDatabase, and exposed to render tasks under the
        // REND_RES_STATIC_{VERTICES,INDICES} global IDs.
        const auto& triangles_info = WBE::RenderDatabase::get_singleton()->get_static_triangle_set_info();
        if (triangles_info.draw_groups.size() != 1) {
            throw std::runtime_error("Failed to setup draw triangle test: expected one vertex draw group.");
        }
        auto shader_resource = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUShader>("draw_triangle_shader");
        WBE::VertexInputDescriptionVK vertex_input = WBE::build_vertex_input_description_vk(
            WBE::ShaderMetadataLoader().load(shader_resource), "vertex_main", triangles_info.draw_groups[0].vertex_layout);
        const VkDeviceSize vertex_buffer_offset = triangles_info.draw_groups[0].vertex_buffer_byte_offset;
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.add_resource(REND_RES_TEST_DRAW_TRIANGLE_GRAPHICS_PIPELINE, graphics_pipeline);
        WBE::RendTaskDescVK task_desc(render_allocator);
        task_desc.task_id = RT_RUN_PIPELINE;
        task_desc.create = [render_allocator, base_render_data, vertex_input, vertex_buffer_offset]() -> WBE::Unique<WBE::RenderTaskVK> {
            return WBE::make_unique<WBE::RTRunGraphicsPipelineVK>(render_allocator,
                base_render_data,
                glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
                base_render_data->swap_chain_extent,
                WBE::REND_RES_SWAP_CHAIN_IMAGE,
                REND_RES_TEST_DRAW_TRIANGLE_GRAPHICS_PIPELINE,
                WBE::REND_RES_STATIC_VERTICES,
                WBE::REND_RES_STATIC_INDICES,
                std::nullopt,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                vertex_input,
                vertex_buffer_offset);
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
        WBE::RenderDatabase::get_singleton()->set_static_triangle_set(triangle_mesh.get());
        game_application.get_renderer()->update_static_triangle_set();
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
