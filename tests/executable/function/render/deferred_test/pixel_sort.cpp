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
    float min_metric = PixelSort::MIN_METRIC;
    float max_metric = PixelSort::MAX_METRIC;
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

struct PixelSort::Layout {
    VkExtent2D extent{};
    // The number of sorted lines: the width for vertical sorting, the height for horizontal sorting.
    uint32_t line_count = 0;
    uint32_t sub_histogram_num = 1;
    size_t column_offsets_buffer_size = 0;
    size_t key_buffer_size = 0;
    size_t histogram_buffer_size = 0;
    VkExtent3D tile_group_count{};
    VkExtent3D column_group_count{};
    VkExtent3D sub_histogram_group_count{};
    VkExtent3D single_group_count{.width = 1, .height = 1, .depth = 1};
};

PixelSort::Layout PixelSort::make_layout(VkExtent2D p_extent, PixelSortDirection p_direction) {
    const bool vertical = p_direction == PixelSortDirection::VERTICAL;
    const uint32_t line_length = vertical ? p_extent.height : p_extent.width;
    const uint64_t element_count = static_cast<uint64_t>(p_extent.width) * p_extent.height;
    // A key packs the line, saturation, and position along the line, so at least 2 saturation levels must fit in 32
    // bits.
    // The mask image stores each masked pixel's rank within its line, and with the span scope its span offset and span
    // length, as 16-bit channels, with 0xFFFF marking unmasked pixels.
    if (line_length >= UINT16_MAX) {
        throw std::runtime_error(std::format("Failed to lay out the pixel sort post-processing: a {}-pixel sorted line is too long for the 16-bit pixel sort mask.",
            line_length));
    }
    if (element_count == 0) {
        throw std::runtime_error(std::format(
            "Failed to lay out the pixel sort post-processing: a {}x{} image is empty.", p_extent.width, p_extent.height));
    }
    if (UINT32_MAX / element_count < 2) {
        throw std::runtime_error(std::format("Failed to lay out the pixel sort post-processing: a {}x{} image is too large to pack its sort keys into 32 bits.",
            p_extent.width,
            p_extent.height));
    }
    Layout layout;
    layout.extent = p_extent;
    layout.line_count = vertical ? p_extent.width : p_extent.height;
    layout.sub_histogram_num = std::clamp(
        static_cast<uint32_t>((element_count + RADIX_SORT_ELEMENTS_PER_WORKGROUP - 1) / RADIX_SORT_ELEMENTS_PER_WORKGROUP),
        1U,
        RADIX_SORT_MAX_WORKGROUP_COUNT);
    // The line counts are followed by the zero element the prefix sum reads.
    layout.column_offsets_buffer_size = (PIXEL_SORT_COLUMN_OFFSETS_START + layout.line_count + 1) * sizeof(uint32_t);
    // Sized for the case where every pixel is inside the mask.
    layout.key_buffer_size = (RADIX_SORT_DATA_START + element_count) * sizeof(uint32_t);
    layout.histogram_buffer_size =
        (RADIX_SORT_DATA_START + static_cast<size_t>(layout.sub_histogram_num) * RADIX_SORT_BUCKET_COUNT) * sizeof(uint32_t);
    layout.tile_group_count = VkExtent3D{.width = (p_extent.width + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
        .height = (p_extent.height + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
        .depth = 1};
    layout.column_group_count = VkExtent3D{
        .width = (layout.line_count + PIXEL_SORT_COLUMN_WORKGROUP_SIZE - 1) / PIXEL_SORT_COLUMN_WORKGROUP_SIZE, .height = 1, .depth = 1};
    layout.sub_histogram_group_count = VkExtent3D{.width = layout.sub_histogram_num, .height = 1, .depth = 1};
    return layout;
}

PixelSort::PixelSort(RenderAllocator* p_allocator,
    RendererVK* p_renderer,
    RenderTestExtentFactory p_extent,
    VkFormat p_color_format,
    PixelSortDirection p_direction,
    PixelSortMetric p_metric,
    PixelSortScope p_scope)
    : IPostProcessing(p_allocator), renderer(p_renderer), extent(std::move(p_extent)), color_format(p_color_format),
      direction(p_direction), metric(p_metric), scope(p_scope) {
    if (renderer == nullptr) {
        throw std::runtime_error("Failed to create the pixel sort post-processing: renderer is null.");
    }
    if (extent == nullptr) {
        throw std::runtime_error("Failed to create the pixel sort post-processing: extent factory is null.");
    }
    if (metric != PixelSortMetric::LUMINANCE && metric != PixelSortMetric::HUE && metric != PixelSortMetric::SATURATION) {
        throw std::runtime_error(std::format("Failed to create the pixel sort post-processing: {} is not a pixel sort metric.",
            static_cast<uint32_t>(metric)));
    }
    if (scope != PixelSortScope::LINE && scope != PixelSortScope::SPAN) {
        throw std::runtime_error(std::format("Failed to create the pixel sort post-processing: {} is not a pixel sort scope.",
            static_cast<uint32_t>(scope)));
    }
    // Reject an extent that is invalid for the current renderer state before any graph is built.
    static_cast<void>(make_layout(extent(RendGraphContextVK::from_render_data(*renderer->get_base_render_data())), direction));
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
    // Every factory evaluates the layout of the extent in the graph's context, and captures copies rather than this,
    // because graphs may outlive the effect.
    auto layout_of = [extent_factory = extent, sort_direction = direction](const RendGraphContextVK& p_context) {
        return make_layout(extent_factory(p_context), sort_direction);
    };
    auto storage_buffer = [layout_of](size_t Layout::* p_size) {
        return [layout_of, p_size](const RendGraphContextVK& p_context) {
            return RendBufferDescVK{.usage = RendResPrimaryUsageVK::BUFFER_STORAGE, .size = layout_of(p_context).*p_size};
        };
    };
    auto image = [layout_of](VkFormat p_format) {
        return [layout_of, p_format](const RendGraphContextVK& p_context) {
            const VkExtent2D image_extent = layout_of(p_context).extent;
            return RendImageDescVK{.usage = RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                .format = p_format,
                .extent = {.width = image_extent.width, .height = image_extent.height, .depth = 1}};
        };
    };
    // The span scope stores the rank, span offset, and span length of every masked pixel; the line scope only the rank.
    const VkFormat mask_format = scope == PixelSortScope::SPAN ? VK_FORMAT_R16G16B16A16_UINT : VK_FORMAT_R16_UINT;

    p_graph.add_image(REND_RES_PIXEL_SORT_SOURCE_IMAGE, image(color_format))
        .add_image_view(REND_RES_PIXEL_SORT_SOURCE, REND_RES_PIXEL_SORT_SOURCE_IMAGE)
        .add_image(REND_RES_PIXEL_SORT_MASK_IMAGE, image(mask_format))
        .add_image_view(REND_RES_PIXEL_SORT_MASK, REND_RES_PIXEL_SORT_MASK_IMAGE)
        .add_buffer(REND_RES_PIXEL_SORT_CONTRAST,
            [initial_data = to_initial_data(PixelSortContrastGPU{.sort_metric = static_cast<uint32_t>(metric),
                 .sort_scope = static_cast<uint32_t>(scope)})](const RendGraphContextVK& /*p_context*/) {
                return RendBufferDescVK{
                    .usage = RendResPrimaryUsageVK::BUFFER_UNIFORM, .domain = DeviceDomain::HOST_DEVICE, .initial_data = initial_data};
            })
        .add_buffer(REND_RES_PIXEL_SORT_COLUMN_OFFSETS, storage_buffer(&Layout::column_offsets_buffer_size))
        .add_buffer(REND_RES_PIXEL_SORT_KEYS, storage_buffer(&Layout::key_buffer_size))
        .add_buffer(REND_RES_PIXEL_SORT_KEYS_SWAP, storage_buffer(&Layout::key_buffer_size))
        .add_buffer(REND_RES_PIXEL_SORT_HISTOGRAM, storage_buffer(&Layout::histogram_buffer_size));

    add_compute_task(
        p_graph, RT_COPY_PIXEL_SORT_SOURCE, copy_source_pipeline, &Layout::tile_group_count, {{REND_RES_PIXEL_SORT_INPUT, p_color_image_view}});
    add_compute_task(p_graph, RT_COUNT_PIXEL_SORT_MASK, count_mask_pipeline, &Layout::column_group_count, {});
    // The prefix sums run in a single workgroup.
    add_compute_task(p_graph, RT_PREFIX_SUM_PIXEL_SORT_COLUMNS, prefix_sum_columns_pipeline, &Layout::single_group_count, {});
    add_compute_task(p_graph, RT_REGISTER_PIXEL_SORT_KEYS, register_keys_pipeline, &Layout::column_group_count, {});

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
            &Layout::sub_histogram_group_count,
            {{"input_buffer", input_keys}, {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
        add_compute_task(p_graph,
            prefix_sum_task_ids[pass],
            histogram_prefix_sum_pipeline,
            &Layout::single_group_count,
            {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
        add_compute_task(p_graph,
            scatter_task_ids[pass],
            scatter_pipelines[pass],
            &Layout::sub_histogram_group_count,
            {{"input_buffer", input_keys}, {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}, {"output_buffer", output_keys}});
    }
    // After an even number of passes, the sorted keys are back in the key buffer.
    add_compute_task(p_graph,
        RT_WRITE_SORTED_PIXELS,
        write_sorted_pixels_pipeline,
        &Layout::tile_group_count,
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
    VkExtent3D Layout::* p_group_count,
    std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) const {
    RendTaskDescVK task_desc(get_allocator());
    task_desc.task_id = p_task_id;
    for (const auto& [task_resource_id, graph_resource_id] : p_aliases) {
        task_desc.alias(task_resource_id, graph_resource_id);
    }
    task_desc.factory = [this, pipeline = p_pipeline, p_group_count](const RendGraphContextVK& p_context) {
        const Layout layout = make_layout(extent(p_context), direction);
        return make_toggleable_task(make_unique<RTRunComputePipelineVK>(
            get_allocator(), renderer->get_base_render_data(), pipeline, layout.*p_group_count));
    };
    p_graph.add_task(std::move(task_desc));
}

} // namespace WhiteBirdEngine
