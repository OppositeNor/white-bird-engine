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
#include "pixel_sort.hh"
#include "../render_test.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

constexpr std::string_view REND_RES_PIXEL_SORT_INPUT = "pixel_sort_input";
constexpr std::string_view REND_RES_PIXEL_SORT_SOURCE_IMAGE = "pixel_sort_source_image";
constexpr std::string_view REND_RES_PIXEL_SORT_SOURCE = "pixel_sort_source";
constexpr std::string_view REND_RES_PIXEL_SORT_DESTINATION = "pixel_sort_destination";
constexpr std::string_view REND_RES_PIXEL_SORT_CONTRAST = "pixel_sort_contrast";
constexpr std::string_view REND_RES_PIXEL_SORT_COLUMN_OFFSETS = "pixel_sort_column_offsets";
constexpr std::string_view REND_RES_PIXEL_SORT_MASK_IMAGE = "pixel_sort_mask_image";
constexpr std::string_view REND_RES_PIXEL_SORT_MASK = "pixel_sort_mask";
// The register pass writes and the write pass reads this buffer; the radix sort passes alternate between it and the swap
// buffer.
constexpr std::string_view REND_RES_PIXEL_SORT_KEYS = "pixel_sort_keys";
constexpr std::string_view REND_RES_PIXEL_SORT_KEYS_SWAP = "pixel_sort_keys_swap";
constexpr std::string_view REND_RES_PIXEL_SORT_HISTOGRAM = "pixel_sort_histogram";
// Must match the numthreads of copy_pixel_sort_source.slang and write_sorted_pixels.slang.
constexpr uint32_t PIXEL_SORT_TILE_SIZE = 8;
// Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation per
// sorted line.
constexpr uint32_t PIXEL_SORT_COLUMN_WORKGROUP_SIZE = 64;
// Must match WBE_PIXEL_SORT_COLUMN_OFFSETS_START in pixel_sort_utils.slang.
constexpr uint32_t PIXEL_SORT_COLUMN_OFFSETS_START = 4;
// Must match WBE_RADIX_SORT_BUCKET_COUNT, WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP, WBE_RADIX_SORT_MAX_WORKGROUP_COUNT, and
// WBE_RADIX_SORT_DATA_START in radix_sort_utils.slang.
constexpr uint32_t RADIX_SORT_BUCKET_COUNT = 256;
constexpr uint32_t RADIX_SORT_ELEMENTS_PER_WORKGROUP = 256;
constexpr uint32_t RADIX_SORT_MAX_WORKGROUP_COUNT = 16384;
constexpr uint32_t RADIX_SORT_DATA_START = 4;

// GPU layout of PixelSortContrast in pixel_sort_utils.slang.
struct PixelSortContrastGPU {
    float min_luminance = PixelSort::MIN_LUMINANCE;
    float max_luminance = PixelSort::MAX_LUMINANCE;
    uint32_t sort_metric = static_cast<uint32_t>(PixelSortMetric::SATURATION);
    uint32_t sort_scope = static_cast<uint32_t>(PixelSortScope::LINE);
};

std::string make_bit_region_name(uint32_t p_pass) {
    return std::format("{}_{}", p_pass * 8, (p_pass + 1) * 8);
}

// The id of the variant of a direction-dependent pixel sort shader, e.g. count_pixel_sort_mask_vertical_shader.
std::string make_direction_shader_id(std::string_view p_shader_name, PixelSortDirection p_direction) {
    const std::string_view direction_name = p_direction == PixelSortDirection::VERTICAL ? "vertical" : "horizontal";
    return std::format("{}_{}_shader", p_shader_name, direction_name);
}

} // namespace

PixelSort::PixelSort(RenderAllocator* p_allocator,
    RendererVK* p_renderer,
    VkExtent2D p_extent,
    VkFormat p_color_format,
    PixelSortDirection p_direction,
    PixelSortMetric p_metric,
    PixelSortScope p_scope)
    : IPostProcessing(p_allocator), renderer(p_renderer), extent(p_extent), color_format(p_color_format),
      direction(p_direction), metric(p_metric), scope(p_scope) {
    if (renderer == nullptr) {
        throw std::runtime_error("Failed to create the pixel sort post-processing: renderer is null.");
    }
    if (metric != PixelSortMetric::LUMINANCE && metric != PixelSortMetric::HUE && metric != PixelSortMetric::SATURATION) {
        throw std::runtime_error(std::format("Failed to create the pixel sort post-processing: {} is not a pixel sort metric.",
            static_cast<uint32_t>(metric)));
    }
    if (scope != PixelSortScope::LINE && scope != PixelSortScope::SPAN) {
        throw std::runtime_error(std::format("Failed to create the pixel sort post-processing: {} is not a pixel sort scope.",
            static_cast<uint32_t>(scope)));
    }
    const bool vertical = direction == PixelSortDirection::VERTICAL;
    line_count = vertical ? extent.width : extent.height;
    const uint32_t line_length = vertical ? extent.height : extent.width;
    const uint64_t element_count = static_cast<uint64_t>(extent.width) * extent.height;
    // A key packs the line, saturation, and position along the line, so at least 2 saturation levels must fit in 32
    // bits.
    // The mask image stores each masked pixel's rank within its line, and with the span scope its span offset and span
    // length, as 16-bit channels, with 0xFFFF marking unmasked pixels.
    if (line_length >= UINT16_MAX) {
        throw std::runtime_error(std::format("Failed to create the pixel sort post-processing: a {}-pixel sorted line is too long for the 16-bit pixel sort mask.",
            line_length));
    }
    if (element_count == 0 || UINT32_MAX / element_count < 2) {
        throw std::runtime_error(std::format("Failed to create the pixel sort post-processing: a {}x{} image is empty or too large to pack its sort keys into 32 bits.",
            extent.width, extent.height));
    }
    sub_histogram_num = std::clamp(
        static_cast<uint32_t>((element_count + RADIX_SORT_ELEMENTS_PER_WORKGROUP - 1) / RADIX_SORT_ELEMENTS_PER_WORKGROUP),
        1U,
        RADIX_SORT_MAX_WORKGROUP_COUNT);
    copy_source_pipeline = compile("copy_pixel_sort_source_shader");
    count_mask_pipeline = compile(make_direction_shader_id("count_pixel_sort_mask", direction));
    prefix_sum_columns_pipeline = compile("prefix_sum_pixel_sort_columns_shader");
    register_keys_pipeline = compile(make_direction_shader_id("register_pixel_sort_keys", direction));
    write_sorted_pixels_pipeline = compile(make_direction_shader_id("write_sorted_pixels", direction));
    histogram_prefix_sum_pipeline = compile("radix_sort_histogram_prefix_sum");
    for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
        const std::string bit_region = make_bit_region_name(pass);
        histogram_pipelines[pass] = compile(std::format("radix_sort_histogram_workgroup_process_{}", bit_region));
        scatter_pipelines[pass] = compile(std::format("radix_sort_scatter_{}", bit_region));
    }
}

std::string_view PixelSort::add_render_tasks(
    RendGraphDescVK& p_graph, std::string_view p_color_image_view, std::string_view p_previous_task_id) const {
    const size_t element_count = static_cast<size_t>(extent.width) * extent.height;
    const VkExtent3D tile_group_count{.width = (extent.width + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
        .height = (extent.height + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
        .depth = 1};
    const VkExtent3D column_group_count{
        .width = (line_count + PIXEL_SORT_COLUMN_WORKGROUP_SIZE - 1) / PIXEL_SORT_COLUMN_WORKGROUP_SIZE, .height = 1, .depth = 1};
    // The line counts are followed by the zero element the prefix sum reads.
    const size_t column_offsets_buffer_size = (PIXEL_SORT_COLUMN_OFFSETS_START + line_count + 1) * sizeof(uint32_t);
    // Sized for the case where every pixel is inside the mask.
    const size_t key_buffer_size = (RADIX_SORT_DATA_START + element_count) * sizeof(uint32_t);
    const size_t histogram_buffer_size =
        (RADIX_SORT_DATA_START + static_cast<size_t>(sub_histogram_num) * RADIX_SORT_BUCKET_COUNT) * sizeof(uint32_t);
    const VkExtent3D image_extent{.width = extent.width, .height = extent.height, .depth = 1};
    // The span scope stores the rank, span offset, and span length of every masked pixel; the line scope only the rank.
    const VkFormat mask_format = scope == PixelSortScope::SPAN ? VK_FORMAT_R16G16B16A16_UINT : VK_FORMAT_R16_UINT;

    p_graph
        .add_image(REND_RES_PIXEL_SORT_SOURCE_IMAGE,
            {.usage = RendResPrimaryUsageVK::IMAGE_READ_WRITE, .format = color_format, .extent = image_extent})
        .add_image_view(REND_RES_PIXEL_SORT_SOURCE, REND_RES_PIXEL_SORT_SOURCE_IMAGE)
        .add_image(REND_RES_PIXEL_SORT_MASK_IMAGE,
            {.usage = RendResPrimaryUsageVK::IMAGE_READ_WRITE, .format = mask_format, .extent = image_extent})
        .add_image_view(REND_RES_PIXEL_SORT_MASK, REND_RES_PIXEL_SORT_MASK_IMAGE)
        .add_buffer(REND_RES_PIXEL_SORT_CONTRAST,
            {.usage = RendResPrimaryUsageVK::BUFFER_UNIFORM,
                .domain = DeviceDomain::HOST_DEVICE,
                .initial_data = to_initial_data(PixelSortContrastGPU{
                    .sort_metric = static_cast<uint32_t>(metric), .sort_scope = static_cast<uint32_t>(scope)})})
        .add_buffer(REND_RES_PIXEL_SORT_COLUMN_OFFSETS,
            {.usage = RendResPrimaryUsageVK::BUFFER_STORAGE, .size = column_offsets_buffer_size})
        .add_buffer(REND_RES_PIXEL_SORT_KEYS, {.usage = RendResPrimaryUsageVK::BUFFER_STORAGE, .size = key_buffer_size})
        .add_buffer(REND_RES_PIXEL_SORT_KEYS_SWAP, {.usage = RendResPrimaryUsageVK::BUFFER_STORAGE, .size = key_buffer_size})
        .add_buffer(REND_RES_PIXEL_SORT_HISTOGRAM, {.usage = RendResPrimaryUsageVK::BUFFER_STORAGE, .size = histogram_buffer_size});

    add_compute_task(p_graph,
        RT_COPY_PIXEL_SORT_SOURCE,
        copy_source_pipeline,
        tile_group_count,
        {{REND_RES_PIXEL_SORT_INPUT, p_color_image_view}});
    add_compute_task(p_graph, RT_COUNT_PIXEL_SORT_MASK, count_mask_pipeline, column_group_count, {});
    // The prefix sums run in a single workgroup.
    add_compute_task(p_graph, RT_PREFIX_SUM_PIXEL_SORT_COLUMNS, prefix_sum_columns_pipeline, {.width = 1, .height = 1, .depth = 1}, {});
    add_compute_task(p_graph, RT_REGISTER_PIXEL_SORT_KEYS, register_keys_pipeline, column_group_count, {});

    std::array<std::string, RADIX_SORT_PASS_COUNT> histogram_task_ids;
    std::array<std::string, RADIX_SORT_PASS_COUNT> prefix_sum_task_ids;
    std::array<std::string, RADIX_SORT_PASS_COUNT> scatter_task_ids;
    for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
        const std::string_view input_keys = pass % 2 == 0 ? REND_RES_PIXEL_SORT_KEYS : REND_RES_PIXEL_SORT_KEYS_SWAP;
        const std::string_view output_keys = pass % 2 == 0 ? REND_RES_PIXEL_SORT_KEYS_SWAP : REND_RES_PIXEL_SORT_KEYS;
        const std::string bit_region = make_bit_region_name(pass);
        histogram_task_ids[pass] = std::format("pixel_sort_radix_sort_histogram_{}", bit_region);
        prefix_sum_task_ids[pass] = std::format("pixel_sort_radix_sort_histogram_prefix_sum_{}", bit_region);
        scatter_task_ids[pass] = std::format("pixel_sort_radix_sort_scatter_{}", bit_region);
        add_compute_task(p_graph,
            histogram_task_ids[pass],
            histogram_pipelines[pass],
            {.width = sub_histogram_num, .height = 1, .depth = 1},
            {{"input_buffer", input_keys}, {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
        add_compute_task(p_graph,
            prefix_sum_task_ids[pass],
            histogram_prefix_sum_pipeline,
            {.width = 1, .height = 1, .depth = 1},
            {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
        add_compute_task(p_graph,
            scatter_task_ids[pass],
            scatter_pipelines[pass],
            {.width = sub_histogram_num, .height = 1, .depth = 1},
            {{"input_buffer", input_keys}, {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}, {"output_buffer", output_keys}});
    }
    // After an even number of passes, the sorted keys are back in the key buffer.
    add_compute_task(p_graph,
        RT_WRITE_SORTED_PIXELS,
        write_sorted_pixels_pipeline,
        tile_group_count,
        {{REND_RES_PIXEL_SORT_DESTINATION, p_color_image_view}});

    connect_resource_chain(p_graph, p_color_image_view, {p_previous_task_id, RT_COPY_PIXEL_SORT_SOURCE, RT_WRITE_SORTED_PIXELS});
    connect_resource_chain(p_graph,
        REND_RES_PIXEL_SORT_SOURCE,
        {RT_COPY_PIXEL_SORT_SOURCE, RT_COUNT_PIXEL_SORT_MASK, RT_REGISTER_PIXEL_SORT_KEYS, RT_WRITE_SORTED_PIXELS});
    connect_resource_chain(p_graph,
        REND_RES_PIXEL_SORT_COLUMN_OFFSETS,
        {RT_COUNT_PIXEL_SORT_MASK, RT_PREFIX_SUM_PIXEL_SORT_COLUMNS, RT_REGISTER_PIXEL_SORT_KEYS, RT_WRITE_SORTED_PIXELS});
    connect_resource_chain(p_graph, REND_RES_PIXEL_SORT_MASK, {RT_REGISTER_PIXEL_SORT_KEYS, RT_WRITE_SORTED_PIXELS});
    // Every task using a key buffer or the histogram, in execution order. Pass p reads the keys from one buffer and scatters
    // them into the other, so each buffer is written by every other scatter.
    connect_resource_chain(p_graph,
        REND_RES_PIXEL_SORT_KEYS,
        {RT_REGISTER_PIXEL_SORT_KEYS,
            histogram_task_ids[0],
            scatter_task_ids[0],
            scatter_task_ids[1],
            histogram_task_ids[2],
            scatter_task_ids[2],
            scatter_task_ids[3],
            RT_WRITE_SORTED_PIXELS});
    connect_resource_chain(p_graph,
        REND_RES_PIXEL_SORT_KEYS_SWAP,
        {scatter_task_ids[0], histogram_task_ids[1], scatter_task_ids[1], scatter_task_ids[2], histogram_task_ids[3], scatter_task_ids[3]});
    std::array<std::string_view, RADIX_SORT_PASS_COUNT * 3> histogram_chain;
    for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
        histogram_chain[pass * 3 + 0] = histogram_task_ids[pass];
        histogram_chain[pass * 3 + 1] = prefix_sum_task_ids[pass];
        histogram_chain[pass * 3 + 2] = scatter_task_ids[pass];
    }
    connect_resource_chain(p_graph, REND_RES_PIXEL_SORT_HISTOGRAM, histogram_chain);
    return RT_WRITE_SORTED_PIXELS;
}

Ref<ROComputePipelineVK> PixelSort::compile(std::string_view p_shader_id) const {
    auto shader_resource = ResourceManager::get_singleton()->get_resource<RUShader>(p_shader_id);
    if (shader_resource == nullptr) {
        throw std::runtime_error(std::format("Failed to create the pixel sort post-processing: shader resource {} was not found.", p_shader_id));
    }
    ComputePipelineCompilerVKContext compute_compiler_context{
        .allocator = get_allocator(),
        .render_data = renderer->get_base_render_data(),
        .render_database = renderer->get_render_database(),
    };
    return ComputePipelineCompilerVK(compute_compiler_context).compile(p_shader_id, shader_resource).compute_pipeline;
}

void PixelSort::add_compute_task(RendGraphDescVK& p_graph,
    std::string_view p_task_id,
    const Ref<ROComputePipelineVK>& p_pipeline,
    VkExtent3D p_dispatch_group_count,
    std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) const {
    RendTaskDescVK task_desc(get_allocator());
    task_desc.task_id = p_task_id;
    for (const auto& [task_resource_id, graph_resource_id] : p_aliases) {
        task_desc.alias(task_resource_id, graph_resource_id);
    }
    task_desc.task = make_toggleable_task(make_unique<RTRunComputePipelineVK>(
        get_allocator(), renderer->get_base_render_data(), p_pipeline, p_dispatch_group_count));
    p_graph.add_task(std::move(task_desc));
}

} // namespace WhiteBirdEngine
