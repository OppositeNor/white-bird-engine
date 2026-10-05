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
#include "function/render_vk/ro_buffer_vk.hh"
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
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

inline constexpr std::string_view RT_COUNT_PIXEL_SORT_MASK = "count_pixel_sort_mask";
inline constexpr std::string_view RT_PREFIX_SUM_PIXEL_SORT_COLUMNS = "prefix_sum_pixel_sort_columns";
inline constexpr std::string_view RT_REGISTER_PIXEL_SORT_KEYS = "register_pixel_sort_keys";
inline constexpr std::string_view RT_WRITE_SORTED_PIXELS = "write_sorted_pixels";
inline constexpr std::string_view RT_PRESENT_SORTED_PIXELS = "present_sorted_pixels";
inline constexpr std::string_view REND_RES_PIXEL_SORT_SOURCE = "pixel_sort_source";
inline constexpr std::string_view REND_RES_PIXEL_SORT_DESTINATION_IMAGE = "pixel_sort_destination_image";
inline constexpr std::string_view REND_RES_PIXEL_SORT_DESTINATION = "pixel_sort_destination";
inline constexpr std::string_view REND_RES_PIXEL_SORT_SAMPLER = "pixel_sort_sampler";
inline constexpr std::string_view REND_RES_PIXEL_SORT_CONTRAST = "pixel_sort_contrast";
inline constexpr std::string_view REND_RES_PIXEL_SORT_COLUMN_OFFSETS = "pixel_sort_column_offsets";
inline constexpr std::string_view REND_RES_PIXEL_SORT_MASK_IMAGE = "pixel_sort_mask_image";
inline constexpr std::string_view REND_RES_PIXEL_SORT_MASK = "pixel_sort_mask";
// The register pass writes and the write pass reads this buffer; the radix sort passes alternate between it and the swap buffer.
inline constexpr std::string_view REND_RES_PIXEL_SORT_KEYS = "pixel_sort_keys";
inline constexpr std::string_view REND_RES_PIXEL_SORT_KEYS_SWAP = "pixel_sort_keys_swap";
inline constexpr std::string_view REND_RES_PIXEL_SORT_HISTOGRAM = "pixel_sort_histogram";
inline constexpr VkFormat PIXEL_SORT_DESTINATION_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
// Must match the numthreads of write_sorted_pixels.slang.
inline constexpr uint32_t PIXEL_SORT_TILE_SIZE = 8;
// Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation per
// column.
inline constexpr uint32_t PIXEL_SORT_COLUMN_WORKGROUP_SIZE = 64;
// Must match WBE_PIXEL_SORT_COLUMN_OFFSETS_START in pixel_sort_utils.slang.
inline constexpr uint32_t PIXEL_SORT_COLUMN_OFFSETS_START = 4;
inline constexpr float PIXEL_SORT_DEFAULT_MIN_LUMINANCE = 0.25F;
inline constexpr float PIXEL_SORT_DEFAULT_MAX_LUMINANCE = 0.8F;
// Must match WBE_PIXEL_SORT_METRIC_SATURATION in pixel_sort_utils.slang: 0 sorts by luminance, 1 by hue, 2 by saturation.
inline constexpr uint32_t PIXEL_SORT_DEFAULT_METRIC = 2;
// Must match WBE_RADIX_SORT_BUCKET_COUNT, WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP, WBE_RADIX_SORT_MAX_WORKGROUP_COUNT, and
// WBE_RADIX_SORT_DATA_START in radix_sort_utils.slang.
inline constexpr uint32_t RADIX_SORT_BUCKET_COUNT = 256;
inline constexpr uint32_t RADIX_SORT_ELEMENTS_PER_WORKGROUP = 256;
inline constexpr uint32_t RADIX_SORT_MAX_WORKGROUP_COUNT = 16384;
inline constexpr uint32_t RADIX_SORT_DATA_START = 4;
// Each pass sorts by one 8-bit digit of the 32-bit keys.
inline constexpr uint32_t RADIX_SORT_PASS_COUNT = 4;

/**
 * @class PixelSortContrastGPU
 * @brief GPU layout of PixelSortContrast in pixel_sort_utils.slang.
 */
struct PixelSortContrastGPU {
    /** The smallest luminance of a sorted pixel, in [0, 1]. */
    float min_luminance = PIXEL_SORT_DEFAULT_MIN_LUMINANCE;
    /** The largest luminance of a sorted pixel, in [0, 1]. */
    float max_luminance = PIXEL_SORT_DEFAULT_MAX_LUMINANCE;
    /** The metric the sorted pixels are ordered by: 0 for luminance, 1 for hue, 2 for saturation. */
    uint32_t sort_metric = PIXEL_SORT_DEFAULT_METRIC;
};

/**
 * @class ApplicationVulkanPixelSortingTest
 * @brief Sorts the pixels of every column of an image that are inside a luminance contrast mask with the GPU radix sort,
 * and presents the result.
 *
 * One invocation per column counts its masked pixels, a prefix sum turns the counts into the range of every column in
 * the key buffer, and one invocation per column registers its masked pixels into that range and stores each pixel's
 * buffer index in an index image. Every key orders by column, then saturation, and stores the source row (see
 * pixel_sort_utils.slang), so one radix sort of all keys sorts every column in place within its range. Each of the 4 radix
 * sort passes runs the histogram, histogram prefix sum, and scatter shaders for one 8-bit digit, alternating between two
 * key buffers. Finally, every masked pixel reads the sorted key at its stored index, and the other pixels keep their color.
 */
class ApplicationVulkanPixelSortingTest : public WBE::ApplicationGame {
public:
    ApplicationVulkanPixelSortingTest()
        : WBE::ApplicationGame({}, WBE::make_ref<WBE::InputManagerMocked>(WBE::global_allocator(), WBE::global_allocator())) {
    }

    virtual void logic_update(double p_delta) override {
    }

    virtual void render_update(double /*p_delta*/) override {
        auto* renderer = WBE::RendererVK::get_singleton();
        WBE::on_exec_test_render(*renderer, renderer->get_base_render_data()->current_frame);
    }

    /**
     * @brief Set the luminance range of the sorted pixels.
     *
     * Before setup_render_graph(), the range becomes the initial contrast buffer content; afterwards the buffer is updated
     * through a render job. Must be called from the logic thread.
     *
     * @param p_min_luminance The smallest luminance of a sorted pixel.
     * @param p_max_luminance The largest luminance of a sorted pixel.
     * @throws std::runtime_error If the range is not within [0, 1] or p_min_luminance exceeds p_max_luminance.
     */
    void set_contrast(float p_min_luminance, float p_max_luminance) {
        if (!(0.0F <= p_min_luminance && p_min_luminance <= p_max_luminance && p_max_luminance <= 1.0F)) {
            throw std::runtime_error(std::format("Invalid pixel sort contrast range [{}, {}]: it must satisfy 0 <= min <= max <= 1.",
                p_min_luminance, p_max_luminance));
        }
        contrast.min_luminance = p_min_luminance;
        contrast.max_luminance = p_max_luminance;
        WBE::ROBufferVK* contrast_buffer_ptr = contrast_buffer.get();
        if (contrast_buffer_ptr == nullptr) {
            return;
        }
        const PixelSortContrastGPU contrast_value = contrast;
        add_render_job([contrast_value, contrast_buffer_ptr]() {
            contrast_buffer_ptr->update_buffer(&contrast_value, sizeof(PixelSortContrastGPU));
        });
    }

    void setup_render_graph() {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();
        auto* resource_manager = WBE::ResourceManager::get_singleton();

        auto texture_resource = resource_manager->get_resource<WBE::RUTexture>(REND_RES_PIXEL_SORT_SOURCE);
        if (texture_resource == nullptr) {
            throw std::runtime_error("Failed to setup pixel sorting test: source texture resource was not found.");
        }
        auto present_pipeline_resource = resource_manager->get_resource<WBE::RUGraphicsPipeline>("present_pixel_sort_pipeline");
        if (present_pipeline_resource == nullptr) {
            throw std::runtime_error("Failed to setup pixel sorting test: present pipeline resource was not found.");
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

        const VkExtent3D source_extent = source_view->fields().image->fields().extent;
        const uint64_t element_count = static_cast<uint64_t>(source_extent.width) * source_extent.height;
        // A key packs the column, saturation, and row, so at least 2 saturation levels must fit in 32 bits.
        // The mask image stores each masked pixel's rank within its column as R16_UINT, with 0xFFFF marking unmasked pixels.
        if (source_extent.height >= UINT16_MAX) {
            throw std::runtime_error(std::format("Failed to setup pixel sorting test: a {}-pixel-high image is too high for the 16-bit pixel sort mask.",
                source_extent.height));
        }
        if (element_count == 0 || UINT32_MAX / element_count < 2) {
            throw std::runtime_error(std::format("Failed to setup pixel sorting test: a {}x{} image is too large to pack its pixel sort keys into 32 bits.",
                source_extent.width, source_extent.height));
        }
        const uint32_t sub_histogram_num = std::clamp(
            static_cast<uint32_t>((element_count + RADIX_SORT_ELEMENTS_PER_WORKGROUP - 1) / RADIX_SORT_ELEMENTS_PER_WORKGROUP),
            1U,
            RADIX_SORT_MAX_WORKGROUP_COUNT);
        const VkExtent3D tile_group_count{.width = (source_extent.width + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
            .height = (source_extent.height + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
            .depth = 1};
        const uint32_t column_group_count = (source_extent.width + PIXEL_SORT_COLUMN_WORKGROUP_SIZE - 1) / PIXEL_SORT_COLUMN_WORKGROUP_SIZE;
        // The column counts are followed by the zero element the prefix sum reads.
        const size_t column_offsets_buffer_size = (PIXEL_SORT_COLUMN_OFFSETS_START + source_extent.width + 1) * sizeof(uint32_t);
        // Sized for the case where every pixel is inside the mask.
        const size_t key_buffer_size = (RADIX_SORT_DATA_START + element_count) * sizeof(uint32_t);
        const size_t histogram_buffer_size =
            (RADIX_SORT_DATA_START + static_cast<size_t>(sub_histogram_num) * RADIX_SORT_BUCKET_COUNT) * sizeof(uint32_t);

        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        pipeline_desc.add_resource(WBE::REND_RES_SWAP_CHAIN, *renderer->acquire_swap_chain_image_view_container())
            .add_resource(REND_RES_PIXEL_SORT_SOURCE, *source_view)
            .add_buffer(REND_RES_PIXEL_SORT_CONTRAST,
                {.usage = WBE::RendResPrimaryUsageVK::BUFFER_UNIFORM,
                    .domain = WBE::DeviceDomain::HOST_DEVICE,
                    .initial_data = WBE::to_initial_data(contrast)})
            .add_buffer(REND_RES_PIXEL_SORT_COLUMN_OFFSETS,
                {.usage = WBE::RendResPrimaryUsageVK::BUFFER_STORAGE, .size = column_offsets_buffer_size})
            .add_buffer(REND_RES_PIXEL_SORT_KEYS, {.usage = WBE::RendResPrimaryUsageVK::BUFFER_STORAGE, .size = key_buffer_size})
            .add_buffer(REND_RES_PIXEL_SORT_KEYS_SWAP, {.usage = WBE::RendResPrimaryUsageVK::BUFFER_STORAGE, .size = key_buffer_size})
            .add_buffer(REND_RES_PIXEL_SORT_HISTOGRAM,
                {.usage = WBE::RendResPrimaryUsageVK::BUFFER_STORAGE, .size = histogram_buffer_size})
            .add_image(REND_RES_PIXEL_SORT_DESTINATION_IMAGE,
                {.usage = WBE::RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                    .format = PIXEL_SORT_DESTINATION_FORMAT,
                    .extent = {.width = source_extent.width, .height = source_extent.height, .depth = 1}})
            .add_image_view(REND_RES_PIXEL_SORT_DESTINATION, REND_RES_PIXEL_SORT_DESTINATION_IMAGE)
            .add_image(REND_RES_PIXEL_SORT_MASK_IMAGE,
                {.usage = WBE::RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                    .format = VK_FORMAT_R16_UINT,
                    .extent = {.width = source_extent.width, .height = source_extent.height, .depth = 1}})
            .add_image_view(REND_RES_PIXEL_SORT_MASK, REND_RES_PIXEL_SORT_MASK_IMAGE)
            .add_sampler(REND_RES_PIXEL_SORT_SAMPLER,
                {.address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE});

        const VkExtent3D column_dispatch{.width = column_group_count, .height = 1, .depth = 1};
        add_compute_task(pipeline_desc, RT_COUNT_PIXEL_SORT_MASK, "count_pixel_sort_mask_vertical_shader", column_dispatch, {});
        // The prefix sum runs in a single workgroup.
        add_compute_task(pipeline_desc,
            RT_PREFIX_SUM_PIXEL_SORT_COLUMNS,
            "prefix_sum_pixel_sort_columns_shader",
            {.width = 1, .height = 1, .depth = 1},
            {});
        add_compute_task(pipeline_desc, RT_REGISTER_PIXEL_SORT_KEYS, "register_pixel_sort_keys_vertical_shader", column_dispatch, {});

        std::array<std::string, RADIX_SORT_PASS_COUNT> histogram_task_ids;
        std::array<std::string, RADIX_SORT_PASS_COUNT> prefix_sum_task_ids;
        std::array<std::string, RADIX_SORT_PASS_COUNT> scatter_task_ids;
        for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
            const std::string_view input_keys = pass % 2 == 0 ? REND_RES_PIXEL_SORT_KEYS : REND_RES_PIXEL_SORT_KEYS_SWAP;
            const std::string_view output_keys = pass % 2 == 0 ? REND_RES_PIXEL_SORT_KEYS_SWAP : REND_RES_PIXEL_SORT_KEYS;
            const std::string bit_region = std::format("{}_{}", pass * 8, (pass + 1) * 8);
            histogram_task_ids[pass] = std::format("radix_sort_histogram_{}", bit_region);
            prefix_sum_task_ids[pass] = std::format("radix_sort_histogram_prefix_sum_{}", bit_region);
            scatter_task_ids[pass] = std::format("radix_sort_scatter_{}", bit_region);
            add_compute_task(pipeline_desc,
                histogram_task_ids[pass],
                std::format("radix_sort_histogram_workgroup_process_{}", bit_region),
                {.width = sub_histogram_num, .height = 1, .depth = 1},
                {{"input_buffer", input_keys}, {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
            // The prefix sum runs in a single workgroup.
            add_compute_task(pipeline_desc,
                prefix_sum_task_ids[pass],
                "radix_sort_histogram_prefix_sum",
                {.width = 1, .height = 1, .depth = 1},
                {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
            add_compute_task(pipeline_desc,
                scatter_task_ids[pass],
                std::format("radix_sort_scatter_{}", bit_region),
                {.width = sub_histogram_num, .height = 1, .depth = 1},
                {{"input_buffer", input_keys},
                    {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM},
                    {"output_buffer", output_keys}});
        }

        // After an even number of passes, the sorted keys are back in the key buffer.
        add_compute_task(pipeline_desc, RT_WRITE_SORTED_PIXELS, "write_sorted_pixels_vertical_shader", tile_group_count, {});

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

        WBE::connect_resource_chain(pipeline_desc,
            REND_RES_PIXEL_SORT_COLUMN_OFFSETS,
            {RT_COUNT_PIXEL_SORT_MASK, RT_PREFIX_SUM_PIXEL_SORT_COLUMNS, RT_REGISTER_PIXEL_SORT_KEYS, RT_WRITE_SORTED_PIXELS});
        WBE::connect_resource_chain(pipeline_desc, REND_RES_PIXEL_SORT_MASK, {RT_REGISTER_PIXEL_SORT_KEYS, RT_WRITE_SORTED_PIXELS});
        // Every task using a key buffer or the histogram, in execution order. Pass p reads the keys from one buffer and
        // scatters them into the other, so each buffer is written by every other scatter.
        WBE::connect_resource_chain(pipeline_desc,
            REND_RES_PIXEL_SORT_KEYS,
            {RT_REGISTER_PIXEL_SORT_KEYS,
                histogram_task_ids[0],
                scatter_task_ids[0],
                scatter_task_ids[1],
                histogram_task_ids[2],
                scatter_task_ids[2],
                scatter_task_ids[3],
                RT_WRITE_SORTED_PIXELS});
        WBE::connect_resource_chain(pipeline_desc,
            REND_RES_PIXEL_SORT_KEYS_SWAP,
            {scatter_task_ids[0],
                histogram_task_ids[1],
                scatter_task_ids[1],
                scatter_task_ids[2],
                histogram_task_ids[3],
                scatter_task_ids[3]});
        std::array<std::string_view, RADIX_SORT_PASS_COUNT * 3> histogram_chain;
        for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
            histogram_chain[pass * 3 + 0] = histogram_task_ids[pass];
            histogram_chain[pass * 3 + 1] = prefix_sum_task_ids[pass];
            histogram_chain[pass * 3 + 2] = scatter_task_ids[pass];
        }
        WBE::connect_resource_chain(pipeline_desc, REND_RES_PIXEL_SORT_HISTOGRAM, histogram_chain);
        WBE::connect_resource_chain(pipeline_desc, REND_RES_PIXEL_SORT_DESTINATION, {RT_WRITE_SORTED_PIXELS, RT_PRESENT_SORTED_PIXELS});
        WBE::Ref<WBE::RenderGraphVK> graph = renderer->register_render_graph("pixel_sorting_graph", std::move(pipeline_desc));
        contrast_buffer = graph->acquire_resource(REND_RES_PIXEL_SORT_CONTRAST)->get().dynamic_cast_ref<WBE::ROBufferVK>();
        WBE::write_descriptor_debug_output("pixel_sorting_graph_descriptors", graph->list_descriptors());
        renderer->activate_render_graph("pixel_sorting_graph");
    }

private:
    PixelSortContrastGPU contrast;
    // Graph-owned contrast buffer, updated from the logic thread through render jobs.
    WBE::Ref<WBE::ROBufferVK> contrast_buffer;

    /**
     * @brief Add a task that dispatches a compute shader resource.
     * @param p_graph The graph descriptor receiving the task.
     * @param p_task_id The task id.
     * @param p_shader_id The resource id of the compute shader.
     * @param p_dispatch_group_count The workgroup counts of the dispatch.
     * @param p_aliases Pairs of a shader resource id and the graph resource it refers to.
     * @throws std::runtime_error If the shader resource is not found.
     */
    void add_compute_task(WBE::RendGraphDescVK& p_graph,
        std::string_view p_task_id,
        std::string_view p_shader_id,
        VkExtent3D p_dispatch_group_count,
        std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();
        auto shader_resource = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUShader>(p_shader_id);
        if (shader_resource == nullptr) {
            throw std::runtime_error(std::format("Failed to setup pixel sorting test: shader resource {} was not found.", p_shader_id));
        }
        WBE::ComputePipelineCompilerVKContext compute_compiler_context{
            .allocator = render_allocator,
            .render_data = base_render_data,
            .render_database = renderer->get_render_database(),
        };
        WBE::Ref<WBE::ROComputePipelineVK> pipeline =
            WBE::ComputePipelineCompilerVK(compute_compiler_context).compile(p_shader_id, shader_resource).compute_pipeline;
        WBE::RendTaskDescVK task_desc(render_allocator);
        task_desc.task_id = p_task_id;
        for (const auto& [task_resource_id, graph_resource_id] : p_aliases) {
            task_desc.alias(task_resource_id, graph_resource_id);
        }
        task_desc.task = WBE::make_unique<WBE::RTRunComputePipelineVK>(render_allocator, base_render_data, pipeline, p_dispatch_group_count);
        p_graph.add_task(std::move(task_desc));
    }
};

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(p_argc, p_argv, WBE::Path("test_env"));
    {
        ApplicationVulkanPixelSortingTest game_application;
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
