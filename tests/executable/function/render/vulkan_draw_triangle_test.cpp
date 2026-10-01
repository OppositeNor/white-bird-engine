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
#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/function_utils.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include "global/global.hh"
#include "input_manager_mocked.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <glm/ext/vector_float4.hpp>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

inline constexpr std::string_view REND_RES_TEST_DRAW_TRIANGLE_VERTEX_BUFFER = "draw_triangle_vertex_buffer";
inline constexpr std::string_view REND_RES_TEST_DRAW_TRIANGLE_INDEX_BUFFER = "draw_triangle_index_buffer";

class ApplicationVulkanDrawPipelineTest : public WBE::ApplicationGame {
public:
    ApplicationVulkanDrawPipelineTest()
        : WBE::ApplicationGame({}, WBE::make_ref<WBE::InputManagerMocked>(WBE::global_allocator(), WBE::global_allocator())) {
    }

    virtual void logic_update(double p_delta) override {
    }

    virtual void render_update(double /*p_delta*/) override {
        auto* renderer = WBE::RendererVK::get_singleton();
        WBE::on_exec_test_render(*renderer, renderer->get_base_render_data()->current_frame);
    }

    void setup_graphics_pipeline() {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();
        auto triangle_mesh = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUMesh3D>("triangle");
        if (triangle_mesh == nullptr || triangle_mesh->submesh_count != 1 || triangle_mesh->submeshes == nullptr) {
            throw std::runtime_error("Failed to setup draw triangle test: expected exactly one triangle submesh.");
        }
        WBE::RefRes<WBE::RUSubmesh3D> triangle_submesh = triangle_mesh->submeshes.get()[0];
        if (triangle_submesh == nullptr || triangle_submesh->vertices.binary.binary == nullptr ||
            triangle_submesh->indices.binary.binary == nullptr) {
            throw std::runtime_error("Failed to setup draw triangle test: triangle geometry storage is missing.");
        }

        auto pipeline_resource = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUGraphicsPipeline>("draw_triangle_pipeline");
        if (pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup draw triangle test: graphics pipeline resource was not found.");
        }
        WBE::GraphicsPipelineCompilerVKContext compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = renderer->get_render_database(),
            .renderer = renderer,
            .color_attachment_formats = {base_render_data->swap_chain_format},
        };
        auto compiled = WBE::GraphicsPipelineCompilerVK(compiler_context).compile("draw_triangle_pipeline", pipeline_resource);
        auto graphics_pipeline = compiled.graphics_pipeline;
        auto shader_resource = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUShader>("draw_triangle_shader");
        WBE::VertexInputDescriptionVK vertex_input = WBE::build_vertex_input_description_vk(
            WBE::ShaderMetadataLoader().load(shader_resource), "vertex_main", triangle_submesh->vertices);

        const char* vertex_data = triangle_submesh->vertices.binary.binary->data.get() + triangle_submesh->vertices.binary.start;
        const char* index_data = triangle_submesh->indices.binary.binary->data.get() + triangle_submesh->indices.binary.start;

        const WBE::IndexedDrawRegionVK indexed_draw_region{
            .first_index = 0,
            .index_count = static_cast<uint32_t>(triangle_submesh->indices.index_count),
            .vertex_offset = 0,
        };
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.add_resource(WBE::REND_RES_SWAP_CHAIN, renderer->acquire_swap_chain_image_view_container())
            .add_buffer(REND_RES_TEST_DRAW_TRIANGLE_VERTEX_BUFFER,
                {.usage = WBE::RendResPrimaryUsageVK::BUFFER_VERTEX,
                    .domain = WBE::DeviceDomain::HOST_DEVICE,
                    .initial_data = std::vector<uint8_t>(vertex_data, vertex_data + triangle_submesh->vertices.binary.size)})
            .add_buffer(REND_RES_TEST_DRAW_TRIANGLE_INDEX_BUFFER,
                {.usage = WBE::RendResPrimaryUsageVK::BUFFER_INDEX,
                    .domain = WBE::DeviceDomain::HOST_DEVICE,
                    .initial_data = std::vector<uint8_t>(index_data, index_data + triangle_submesh->indices.binary.size)});
        WBE::RendTaskDescVK task_desc(render_allocator);
        task_desc.task_id = RT_RUN_PIPELINE;
        task_desc.alias(WBE::RenderTargetsVK::color_id(0), WBE::REND_RES_SWAP_CHAIN);
        const std::array<VkClearValue, 1> clear_values{VkClearValue{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}}};
        task_desc.task = WBE::RTRunGraphicsPipelineVK::Builder(render_allocator)
                             .set_pipeline(graphics_pipeline)
                             .set_render_extent(base_render_data->swap_chain_extent)
                             .set_clear_values(clear_values)
                             .set_indexed_draw(REND_RES_TEST_DRAW_TRIANGLE_VERTEX_BUFFER,
                                 REND_RES_TEST_DRAW_TRIANGLE_INDEX_BUFFER,
                                 vertex_input,
                                 0,
                                 indexed_draw_region)
                             .create_unique(base_render_data);
        pipeline_desc.tasks.emplace_back(std::move(task_desc));
        WBE::Ref<WBE::RenderGraphVK> graph =
            WBE::RendererVK::get_singleton()->register_render_graph("test_render_graph", std::move(pipeline_desc));
        WBE::write_descriptor_debug_output("test_render_graph_graph_descriptors", graph->list_descriptors());
        WBE::RendererVK::get_singleton()->activate_render_graph("test_render_graph");
    }

private:
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Path("test_env"));
    {
        ApplicationVulkanDrawPipelineTest game_application;
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
