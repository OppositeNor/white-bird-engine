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
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
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
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

inline constexpr std::string_view RT_SORT_PIXELS = "sort_pixels";
inline constexpr std::string_view RT_PRESENT_SORTED_PIXELS = "present_sorted_pixels";
inline constexpr std::string_view REND_RES_PIXEL_SORT_SOURCE = "pixel_sort_source";
inline constexpr std::string_view REND_RES_PIXEL_SORT_DESTINATION_IMAGE = "pixel_sort_destination_image";
inline constexpr std::string_view REND_RES_PIXEL_SORT_DESTINATION = "pixel_sort_destination";
inline constexpr std::string_view REND_RES_PIXEL_SORT_SAMPLER = "pixel_sort_sampler";
inline constexpr VkFormat PIXEL_SORT_DESTINATION_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
// Must match WBE_PIXEL_SORT_CHUNK_SIZE in workgroup_pixel_sort.slang.
inline constexpr uint32_t PIXEL_SORT_CHUNK_SIZE = 64;

/**
 * @class ApplicationVulkanWorkgroupPixelSortingTest
 * @brief Sorts the pixels of every row of each 64x64 chunk of an image by luminance, one workgroup per chunk, and
 * presents the result.
 */
class ApplicationVulkanWorkgroupPixelSortingTest : public WBE::ApplicationGame {
public:
    ApplicationVulkanWorkgroupPixelSortingTest()
        : WBE::ApplicationGame({}, WBE::make_ref<WBE::InputManagerMocked>(WBE::global_allocator(), WBE::global_allocator())) {
    }

    virtual void logic_update(double p_delta) override {
    }

    virtual void render_update(double /*p_delta*/) override {
        auto* renderer = WBE::RendererVK::get_singleton();
        WBE::on_exec_test_render(*renderer, renderer->get_base_render_data()->current_frame);
    }

    void setup_render_graph() {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();
        auto* resource_manager = WBE::ResourceManager::get_singleton();

        auto texture_resource = resource_manager->get_resource<WBE::RUTexture>(REND_RES_PIXEL_SORT_SOURCE);
        if (texture_resource == nullptr) {
            throw std::runtime_error("Failed to setup workgroup pixel sorting test: source texture resource was not found.");
        }
        auto sort_shader_resource = resource_manager->get_resource<WBE::RUShader>("workgroup_pixel_sort_shader");
        if (sort_shader_resource == nullptr) {
            throw std::runtime_error("Failed to setup workgroup pixel sorting test: sort shader resource was not found.");
        }
        auto present_pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("present_pixel_sort_pipeline");
        if (present_pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup workgroup pixel sorting test: present pipeline resource was not found.");
        }

        WBE::GraphicsPipelineCompilerVKContext graphics_compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = renderer->get_render_database(),
            .renderer = renderer,
            .color_attachment_formats = {base_render_data->swap_chain_format},
        };
        WBE::GraphicsPipelineCompilerVK graphics_compiler(graphics_compiler_context);
        WBE::Ref<WBE::ROImageViewVK> source_view = graphics_compiler.compile_texture_view(REND_RES_PIXEL_SORT_SOURCE, texture_resource);
        WBE::Ref<WBE::ROGraphicsPipelineVK> present_pipeline =
            graphics_compiler.compile("present_pixel_sort_pipeline", present_pipeline_resource).graphics_pipeline;
        WBE::ComputePipelineCompilerVKContext compute_compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = renderer->get_render_database(),
        };
        WBE::Ref<WBE::ROComputePipelineVK> sort_pipeline =
            WBE::ComputePipelineCompilerVK(compute_compiler_context).compile("workgroup_pixel_sort_shader", sort_shader_resource).compute_pipeline;

        const VkExtent3D source_extent = source_view->fields().image->fields().extent;
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.add_resource(WBE::REND_RES_SWAP_CHAIN, *renderer->acquire_swap_chain_image_view_container())
            .add_resource(REND_RES_PIXEL_SORT_SOURCE, *source_view)
            .add_image(REND_RES_PIXEL_SORT_DESTINATION_IMAGE,
                {.usage = WBE::RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                    .format = PIXEL_SORT_DESTINATION_FORMAT,
                    .extent = {.width = source_extent.width, .height = source_extent.height, .depth = 1}})
            .add_image_view(REND_RES_PIXEL_SORT_DESTINATION, REND_RES_PIXEL_SORT_DESTINATION_IMAGE)
            .add_sampler(REND_RES_PIXEL_SORT_SAMPLER,
                {.address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE});

        // One workgroup per 64x64 chunk.
        WBE::RendTaskDescVK sort_task_desc(render_allocator);
        sort_task_desc.task_id = RT_SORT_PIXELS;
        sort_task_desc.task = WBE::make_unique<WBE::RTRunComputePipelineVK>(render_allocator,
            base_render_data,
            sort_pipeline,
            VkExtent3D{.width = (source_extent.width + PIXEL_SORT_CHUNK_SIZE - 1) / PIXEL_SORT_CHUNK_SIZE,
                .height = (source_extent.height + PIXEL_SORT_CHUNK_SIZE - 1) / PIXEL_SORT_CHUNK_SIZE,
                .depth = 1});
        pipeline_desc.add_task(std::move(sort_task_desc));

        WBE::RendTaskDescVK present_task_desc(render_allocator);
        present_task_desc.task_id = RT_PRESENT_SORTED_PIXELS;
        present_task_desc.alias(WBE::RenderTargetsVK::color_id(0), WBE::REND_RES_SWAP_CHAIN);
        const std::array<VkClearValue, 1> clear_values{VkClearValue{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}}};
        present_task_desc.task = WBE::RTRunGraphicsPipelineVK::Builder(render_allocator)
                                     .set_pipeline(present_pipeline)
                                     .set_render_extent(base_render_data->swap_chain_extent)
                                     .set_clear_values(clear_values)
                                     .set_draw_vertex_count(3)
                                     .create_unique(base_render_data);
        pipeline_desc.add_task(std::move(present_task_desc));

        WBE::connect_resource_chain(pipeline_desc, REND_RES_PIXEL_SORT_DESTINATION, {RT_SORT_PIXELS, RT_PRESENT_SORTED_PIXELS});
        WBE::Ref<WBE::RenderGraphVK> graph = renderer->register_render_graph("workgroup_pixel_sorting_graph", std::move(pipeline_desc));
        WBE::write_descriptor_debug_output("workgroup_pixel_sorting_graph_descriptors", graph->list_descriptors());
        renderer->activate_render_graph("workgroup_pixel_sorting_graph");
    }
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Path("test_env"));
    {
        ApplicationVulkanWorkgroupPixelSortingTest game_application;
        {
            WBE_START_PROFILE(WBE::WBE_CHANNEL_GLOBAL, "Main profiler");
            game_application.setup_render_graph();
            game_application.run();
            size_t renderer_alloc_max = game_application.get_renderer_allocator()->get_internal_fragmentation_tracker();
            WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Renderer allocator max used: {}", renderer_alloc_max));
        }
    }
    const auto& global_profiles = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_CHANNEL_GLOBAL);
    for (const auto& profile : global_profiles) {
        WBE::stdout_log(WBE::WBE_CHANNEL_GLOBAL)->message(std::format("{}: Delta: {} seconds", profile.message, profile.delta));
    }
    return EXIT_SUCCESS;
}
