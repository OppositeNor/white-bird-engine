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
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/function_utils.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_indirect_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "i_post_processing.hh"
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
// The indirect dispatches of the radix sort passes, written from the local sort count of the registered keys.
constexpr std::string_view REND_RES_PIXEL_SORT_RADIX_SORT_DISPATCH = "pixel_sort_radix_sort_dispatch";
constexpr std::string_view RT_RADIX_SORT_PREPARE_DISPATCH = "pixel_sort_radix_sort_prepare_dispatch";
// Must match the numthreads of copy_pixel_sort_source.slang and write_sorted_pixels.slang.
constexpr uint32_t PIXEL_SORT_TILE_SIZE = 8;
// Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation per
// sorted line.
constexpr uint32_t PIXEL_SORT_COLUMN_WORKGROUP_SIZE = 64;
// Must match WBE_PIXEL_SORT_COLUMN_OFFSETS_START in pixel_sort_utils.slang.
constexpr uint32_t PIXEL_SORT_COLUMN_OFFSETS_START = 4;
// Must match WBE_RADIX_SORT_BUCKET_COUNT, WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP, WBE_RADIX_SORT_MAX_WORKGROUP_COUNT,
// WBE_RADIX_SORT_DATA_START,
// WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM, WBE_RADIX_SORT_LOCAL_SORT_DISPATCH_START, and
// WBE_RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_START in radix_sort_utils.slang.
constexpr uint32_t RADIX_SORT_BUCKET_COUNT = 256;
constexpr uint32_t RADIX_SORT_ELEMENTS_PER_WORKGROUP = 256;
constexpr uint32_t RADIX_SORT_MAX_WORKGROUP_COUNT = 16384;
constexpr uint32_t RADIX_SORT_DATA_START = 4;
constexpr uint32_t RADIX_SORT_SCAN_HISTOGRAM_NUM = 16;
constexpr VkDeviceSize RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET = 0;
constexpr VkDeviceSize RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_OFFSET = 3 * sizeof(uint32_t);
constexpr size_t RADIX_SORT_DISPATCH_BUFFER_SIZE = 2 * sizeof(VkDispatchIndirectCommand);

// GPU layout of PixelSortContrast in pixel_sort_utils.slang.
struct PixelSortContrastGPU {
    float min_metric = 0.0F;
    float max_metric = 0.0F;
    uint32_t sort_metric = static_cast<uint32_t>(PixelSortMetric::SATURATION);
    uint32_t sort_scope = static_cast<uint32_t>(PixelSortScope::LINE);
};

PixelSortContrastGPU make_contrast(const PixelSortSettings& p_settings) {
    return PixelSortContrastGPU{
        .min_metric = p_settings.min_metric,
        .max_metric = p_settings.max_metric,
        .sort_metric = static_cast<uint32_t>(p_settings.metric),
        .sort_scope = static_cast<uint32_t>(p_settings.scope),
    };
}

size_t direction_index(PixelSortDirection p_direction) {
    return p_direction == PixelSortDirection::VERTICAL ? 0 : 1;
}

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
    size_t column_offsets_buffer_size = 0;
    size_t key_buffer_size = 0;
    size_t histogram_buffer_size = 0;
    size_t radix_sort_dispatch_buffer_size = RADIX_SORT_DISPATCH_BUFFER_SIZE;
    VkExtent3D tile_group_count{};
    VkExtent3D column_group_count{};
    VkExtent3D single_group_count{.width = 1, .height = 1, .depth = 1};
};

struct PixelSort::RadixSortTaskIds {
    std::array<std::string, RADIX_SORT_PASS_COUNT> histogram_task_ids;
    std::array<std::string, RADIX_SORT_PASS_COUNT> prefix_sum_task_ids;
    std::array<std::string, RADIX_SORT_PASS_COUNT> overflow_prefix_sum_task_ids;
    std::array<std::string, RADIX_SORT_PASS_COUNT> add_overflow_task_ids;
    std::array<std::string, RADIX_SORT_PASS_COUNT> scatter_task_ids;
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
    // The radix sort has one local sort per RADIX_SORT_ELEMENTS_PER_WORKGROUP keys, up to RADIX_SORT_MAX_WORKGROUP_COUNT, and
    // sizes its buffers for the case where every pixel is inside the mask.
    const uint64_t max_local_sort_num = std::clamp<uint64_t>(
        (element_count + RADIX_SORT_ELEMENTS_PER_WORKGROUP - 1) / RADIX_SORT_ELEMENTS_PER_WORKGROUP, 1, RADIX_SORT_MAX_WORKGROUP_COUNT);
    const uint64_t max_overflow_num = (max_local_sort_num + RADIX_SORT_SCAN_HISTOGRAM_NUM - 1) / RADIX_SORT_SCAN_HISTOGRAM_NUM;
    Layout layout;
    layout.extent = p_extent;
    layout.line_count = vertical ? p_extent.width : p_extent.height;
    layout.column_offsets_buffer_size = (PIXEL_SORT_COLUMN_OFFSETS_START + layout.line_count) * sizeof(uint32_t);
    // Sized for the case where every pixel is inside the mask.
    layout.key_buffer_size = (RADIX_SORT_DATA_START + element_count) * sizeof(uint32_t);
    // The metadata, one sub-histogram per local sort, and one overflow histogram per histogram prefix scan workgroup.
    layout.histogram_buffer_size =
        (RADIX_SORT_DATA_START + (max_local_sort_num + max_overflow_num) * RADIX_SORT_BUCKET_COUNT) * sizeof(uint32_t);
    layout.tile_group_count = VkExtent3D{
        .width = (p_extent.width + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
        .height = (p_extent.height + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
        .depth = 1,
    };
    layout.column_group_count = VkExtent3D{
        .width = (layout.line_count + PIXEL_SORT_COLUMN_WORKGROUP_SIZE - 1) / PIXEL_SORT_COLUMN_WORKGROUP_SIZE,
        .height = 1,
        .depth = 1,
    };
    return layout;
}

PixelSort::PixelSort(
    RenderAllocator* p_allocator, RendererVK* p_renderer, RenderTestExtentFactory p_extent, VkFormat p_color_format, const PixelSortSettings& p_settings)
    : IPostProcessing(p_allocator), renderer(p_renderer), extent(std::move(p_extent)), color_format(p_color_format) {
    if (renderer == nullptr) {
        throw std::runtime_error("Failed to create the pixel sort post-processing: renderer is null.");
    }
    if (extent == nullptr) {
        throw std::runtime_error("Failed to create the pixel sort post-processing: extent factory is null.");
    }
    static_cast<void>(set_settings(p_settings));
    copy_source_pipeline = compile("copy_pixel_sort_source_shader");
    count_mask_pipelines = compile_direction_variants("count_pixel_sort_mask");
    prefix_sum_columns_pipeline = compile("prefix_sum_pixel_sort_columns_shader");
    register_keys_pipelines = compile_direction_variants("register_pixel_sort_keys");
    write_sorted_pixels_pipelines = compile_direction_variants("write_sorted_pixels");
    radix_sort_prepare_dispatch_pipeline = compile("radix_sort_prepare_dispatch");
    histogram_prefix_sum_pipeline = compile("radix_sort_histogram_prefix_sum");
    histogram_overflow_prefix_sum_pipeline = compile("radix_sort_histogram_overflow_prefix_sum");
    histogram_add_overflow_pipeline = compile("radix_sort_histogram_add_overflow");
    for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
        const std::string bit_region = make_bit_region_name(pass);
        histogram_pipelines[pass] = compile(std::format("radix_sort_histogram_workgroup_process_{}", bit_region));
        scatter_pipelines[pass] = compile(std::format("radix_sort_scatter_{}", bit_region));
    }
}

void PixelSort::validate_settings(const PixelSortSettings& p_settings) {
    if (p_settings.metric != PixelSortMetric::LUMINANCE && p_settings.metric != PixelSortMetric::HUE &&
        p_settings.metric != PixelSortMetric::SATURATION) {
        throw std::runtime_error(
            std::format("Invalid pixel sort settings: {} is not a pixel sort metric.", static_cast<uint32_t>(p_settings.metric)));
    }
    if (p_settings.scope != PixelSortScope::LINE && p_settings.scope != PixelSortScope::SPAN) {
        throw std::runtime_error(
            std::format("Invalid pixel sort settings: {} is not a pixel sort scope.", static_cast<uint32_t>(p_settings.scope)));
    }
    if (p_settings.direction != PixelSortDirection::VERTICAL && p_settings.direction != PixelSortDirection::HORIZONTAL) {
        throw std::runtime_error(std::format(
            "Invalid pixel sort settings: {} is not a pixel sort direction.", static_cast<uint32_t>(p_settings.direction)));
    }
    if (p_settings.min_metric < 0.0F) {
        throw std::runtime_error(std::format("Invalid pixel sort settings: the minimum metric {} is negative.", p_settings.min_metric));
    }
    if (p_settings.min_metric > p_settings.max_metric) {
        throw std::runtime_error(std::format("Invalid pixel sort settings: the minimum metric {} exceeds the maximum metric {}.",
            p_settings.min_metric,
            p_settings.max_metric));
    }
}

bool PixelSort::set_settings(const PixelSortSettings& p_settings) {
    validate_settings(p_settings);
    // Reject a direction whose lines are too long for the extent of the current renderer state before any graph uses it.
    static_cast<void>(make_layout(extent(RendGraphContextVK::from_render_data(*renderer->get_base_render_data())), p_settings.direction));
    const bool requires_rebuild = p_settings.direction != settings.direction || p_settings.scope != settings.scope;
    settings = p_settings;
    return requires_rebuild;
}

void PixelSort::upload_settings(RenderGraphVK& p_graph) const {
    Ref<ROBufferVK> contrast_buffer = p_graph.acquire_resource(REND_RES_PIXEL_SORT_CONTRAST)->get().dynamic_cast_ref<ROBufferVK>();
    if (contrast_buffer == MEM_NULL) {
        throw std::runtime_error("Failed to upload the pixel sort settings: the graph's contrast resource is not a buffer.");
    }
    const PixelSortContrastGPU contrast = make_contrast(settings);
    contrast_buffer->update_buffer(&contrast, sizeof(contrast));
}

std::string_view PixelSort::add_render_tasks(
    RendGraphDescVK& p_graph, std::string_view p_color_image_view, std::string_view p_previous_task_id) const {
    add_graph_resources(p_graph);
    add_compute_task(
        p_graph, RT_COPY_PIXEL_SORT_SOURCE, copy_source_pipeline, &Layout::tile_group_count, {{REND_RES_PIXEL_SORT_INPUT, p_color_image_view}});
    add_compute_task(p_graph, RT_COUNT_PIXEL_SORT_MASK, count_mask_pipelines, &Layout::column_group_count, {});
    // The prefix sums run in a single workgroup.
    add_compute_task(p_graph, RT_PREFIX_SUM_PIXEL_SORT_COLUMNS, prefix_sum_columns_pipeline, &Layout::single_group_count, {});
    add_compute_task(p_graph, RT_REGISTER_PIXEL_SORT_KEYS, register_keys_pipelines, &Layout::column_group_count, {});

    const RadixSortTaskIds radix_sort_task_ids = add_radix_sort_tasks(p_graph);
    // After an even number of passes, the sorted keys are back in the key buffer.
    add_compute_task(p_graph,
        RT_WRITE_SORTED_PIXELS,
        write_sorted_pixels_pipelines,
        &Layout::tile_group_count,
        {{REND_RES_PIXEL_SORT_DESTINATION, p_color_image_view}});
    connect_render_tasks(p_graph, p_color_image_view, p_previous_task_id, radix_sort_task_ids);
    return RT_WRITE_SORTED_PIXELS;
}

void PixelSort::add_graph_resources(RendGraphDescVK& p_graph) const {
    // Every factory reads the current settings, so rebuilding a graph applies a changed direction or scope. The effect
    // outlives every graph it was added to.
    auto layout_of = [this](const RendGraphContextVK& p_context) {
        return make_layout(extent(p_context), settings.direction);
    };
    auto storage_buffer = [layout_of](size_t Layout::* p_size) {
        return [layout_of, p_size](const RendGraphContextVK& p_context) {
            return RendBufferDescVK{.usage = RendResPrimaryUsageVK::BUFFER_STORAGE, .size = layout_of(p_context).*p_size};
        };
    };
    auto image = [layout_of](VkFormat p_format) {
        return [layout_of, p_format](const RendGraphContextVK& p_context) {
            const VkExtent2D image_extent = layout_of(p_context).extent;
            return RendImageDescVK{
                .usage = RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                .format = p_format,
                .extent = {.width = image_extent.width, .height = image_extent.height, .depth = 1},
            };
        };
    };
    p_graph.add_image(REND_RES_PIXEL_SORT_SOURCE_IMAGE, image(color_format))
        .add_image_view(REND_RES_PIXEL_SORT_SOURCE, REND_RES_PIXEL_SORT_SOURCE_IMAGE)
        .add_image(REND_RES_PIXEL_SORT_MASK_IMAGE,
            [this, layout_of](const RendGraphContextVK& p_context) {
                // The span scope stores the rank, span offset, and span length of every masked pixel; the line scope only
                // the rank.
                const VkFormat mask_format = settings.scope == PixelSortScope::SPAN ? VK_FORMAT_R16G16B16A16_UINT : VK_FORMAT_R16_UINT;
                const VkExtent2D image_extent = layout_of(p_context).extent;
                return RendImageDescVK{
                    .usage = RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                    .format = mask_format,
                    .extent = {.width = image_extent.width, .height = image_extent.height, .depth = 1},
                };
            })
        .add_image_view(REND_RES_PIXEL_SORT_MASK, REND_RES_PIXEL_SORT_MASK_IMAGE)
        // The initial content follows the settings, so a rebuild after a scope change reallocates the buffer with it.
        .add_buffer(REND_RES_PIXEL_SORT_CONTRAST,
            [this](const RendGraphContextVK& /*p_context*/) {
                return RendBufferDescVK{
                    .usage = RendResPrimaryUsageVK::BUFFER_UNIFORM,
                    .domain = DeviceDomain::HOST_DEVICE,
                    .initial_data = to_initial_data(make_contrast(settings)),
                };
            })
        .add_buffer(REND_RES_PIXEL_SORT_COLUMN_OFFSETS, storage_buffer(&Layout::column_offsets_buffer_size))
        .add_buffer(REND_RES_PIXEL_SORT_KEYS, storage_buffer(&Layout::key_buffer_size))
        .add_buffer(REND_RES_PIXEL_SORT_KEYS_SWAP, storage_buffer(&Layout::key_buffer_size))
        .add_buffer(REND_RES_PIXEL_SORT_HISTOGRAM, storage_buffer(&Layout::histogram_buffer_size))
        .add_buffer(REND_RES_PIXEL_SORT_RADIX_SORT_DISPATCH, [](const RendGraphContextVK& /*p_context*/) {
            return RendBufferDescVK{.usage = RendResPrimaryUsageVK::BUFFER_INDIRECT, .size = RADIX_SORT_DISPATCH_BUFFER_SIZE};
        });
}

PixelSort::RadixSortTaskIds PixelSort::add_radix_sort_tasks(RendGraphDescVK& p_graph) const {
    RadixSortTaskIds task_ids;
    // The element count is fixed once the keys are registered, so every pass uses the same dispatches.
    add_compute_task(p_graph,
        RT_RADIX_SORT_PREPARE_DISPATCH,
        radix_sort_prepare_dispatch_pipeline,
        &Layout::single_group_count,
        {{"input_buffer", REND_RES_PIXEL_SORT_KEYS}, {"dispatch_buffer", REND_RES_PIXEL_SORT_RADIX_SORT_DISPATCH}});
    for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
        const std::string_view input_keys = pass % 2 == 0 ? REND_RES_PIXEL_SORT_KEYS : REND_RES_PIXEL_SORT_KEYS_SWAP;
        const std::string_view output_keys = pass % 2 == 0 ? REND_RES_PIXEL_SORT_KEYS_SWAP : REND_RES_PIXEL_SORT_KEYS;
        const std::string bit_region = make_bit_region_name(pass);
        task_ids.histogram_task_ids[pass] = std::format("pixel_sort_radix_sort_histogram_{}", bit_region);
        task_ids.prefix_sum_task_ids[pass] = std::format("pixel_sort_radix_sort_histogram_prefix_sum_{}", bit_region);
        task_ids.overflow_prefix_sum_task_ids[pass] = std::format("pixel_sort_radix_sort_histogram_overflow_prefix_sum_{}", bit_region);
        task_ids.add_overflow_task_ids[pass] = std::format("pixel_sort_radix_sort_histogram_add_overflow_{}", bit_region);
        task_ids.scatter_task_ids[pass] = std::format("pixel_sort_radix_sort_scatter_{}", bit_region);
        add_indirect_compute_task(p_graph,
            task_ids.histogram_task_ids[pass],
            histogram_pipelines[pass],
            RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
            {{"input_buffer", input_keys}, {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
        add_indirect_compute_task(p_graph,
            task_ids.prefix_sum_task_ids[pass],
            histogram_prefix_sum_pipeline,
            RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_OFFSET,
            {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
        // The overflow histograms are scanned in a single workgroup.
        add_compute_task(p_graph,
            task_ids.overflow_prefix_sum_task_ids[pass],
            histogram_overflow_prefix_sum_pipeline,
            &Layout::single_group_count,
            {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
        add_indirect_compute_task(p_graph,
            task_ids.add_overflow_task_ids[pass],
            histogram_add_overflow_pipeline,
            RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
            {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
        add_indirect_compute_task(p_graph,
            task_ids.scatter_task_ids[pass],
            scatter_pipelines[pass],
            RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
            {{"input_buffer", input_keys}, {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}, {"output_buffer", output_keys}});
    }
    return task_ids;
}

void PixelSort::connect_render_tasks(RendGraphDescVK& p_graph,
    std::string_view p_color_image_view,
    std::string_view p_previous_task_id,
    const RadixSortTaskIds& p_task_ids) {
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
        {
            RT_REGISTER_PIXEL_SORT_KEYS,
            RT_RADIX_SORT_PREPARE_DISPATCH,
            p_task_ids.histogram_task_ids[0],
            p_task_ids.scatter_task_ids[0],
            p_task_ids.scatter_task_ids[1],
            p_task_ids.histogram_task_ids[2],
            p_task_ids.scatter_task_ids[2],
            p_task_ids.scatter_task_ids[3],
            RT_WRITE_SORTED_PIXELS,
        });
    connect_resource_chain(p_graph,
        REND_RES_PIXEL_SORT_KEYS_SWAP,
        {
            p_task_ids.scatter_task_ids[0],
            p_task_ids.histogram_task_ids[1],
            p_task_ids.scatter_task_ids[1],
            p_task_ids.scatter_task_ids[2],
            p_task_ids.histogram_task_ids[3],
            p_task_ids.scatter_task_ids[3],
        });
    std::array<std::string_view, RADIX_SORT_PASS_COUNT * 5> histogram_chain;
    // The prepare task followed by every task dispatched indirectly, in execution order.
    std::array<std::string_view, 1 + RADIX_SORT_PASS_COUNT * 4> dispatch_chain;
    dispatch_chain[0] = RT_RADIX_SORT_PREPARE_DISPATCH;
    for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
        histogram_chain[pass * 5 + 0] = p_task_ids.histogram_task_ids[pass];
        histogram_chain[pass * 5 + 1] = p_task_ids.prefix_sum_task_ids[pass];
        histogram_chain[pass * 5 + 2] = p_task_ids.overflow_prefix_sum_task_ids[pass];
        histogram_chain[pass * 5 + 3] = p_task_ids.add_overflow_task_ids[pass];
        histogram_chain[pass * 5 + 4] = p_task_ids.scatter_task_ids[pass];
        dispatch_chain[1 + pass * 4 + 0] = p_task_ids.histogram_task_ids[pass];
        dispatch_chain[1 + pass * 4 + 1] = p_task_ids.prefix_sum_task_ids[pass];
        dispatch_chain[1 + pass * 4 + 2] = p_task_ids.add_overflow_task_ids[pass];
        dispatch_chain[1 + pass * 4 + 3] = p_task_ids.scatter_task_ids[pass];
    }
    connect_resource_chain(p_graph, REND_RES_PIXEL_SORT_HISTOGRAM, histogram_chain);
    connect_resource_chain(p_graph, REND_RES_PIXEL_SORT_RADIX_SORT_DISPATCH, dispatch_chain);
}

Ref<ROComputePipelineVK> PixelSort::compile(std::string_view p_shader_id) const {
    auto shader_resource = ResourceManager::get_singleton()->get_resource<RUShader>(p_shader_id);
    if (shader_resource == nullptr) {
        throw std::runtime_error(
            std::format("Failed to create the pixel sort post-processing: shader resource {} was not found.", p_shader_id));
    }
    ComputePipelineCompilerVKContext compute_compiler_context{
        .allocator = get_allocator(),
        .render_data = renderer->get_base_render_data(),
        .render_database = renderer->get_render_database(),
    };
    return ComputePipelineCompilerVK(compute_compiler_context).compile(p_shader_id, shader_resource).compute_pipeline;
}

PixelSort::DirectionPipelines PixelSort::compile_direction_variants(std::string_view p_shader_name) const {
    DirectionPipelines pipelines;
    pipelines[direction_index(PixelSortDirection::VERTICAL)] =
        compile(make_direction_shader_id(p_shader_name, PixelSortDirection::VERTICAL));
    pipelines[direction_index(PixelSortDirection::HORIZONTAL)] =
        compile(make_direction_shader_id(p_shader_name, PixelSortDirection::HORIZONTAL));
    return pipelines;
}

void PixelSort::add_compute_task(RendGraphDescVK& p_graph,
    std::string_view p_task_id,
    const DirectionPipelines& p_pipelines,
    VkExtent3D Layout::* p_group_count,
    std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) const {
    RendTaskDescVK task_desc(get_allocator());
    task_desc.task_id = p_task_id;
    for (const auto& [task_resource_id, graph_resource_id] : p_aliases) {
        task_desc.alias(task_resource_id, graph_resource_id);
    }
    task_desc.factory = [this, pipelines = p_pipelines, p_group_count](const RendGraphContextVK& p_context) {
        const Layout layout = make_layout(extent(p_context), settings.direction);
        return make_toggleable_task(make_unique<RTRunComputePipelineVK>(get_allocator(),
            renderer->get_base_render_data(),
            pipelines[direction_index(settings.direction)],
            layout.*p_group_count,
            QueueFamilyVK::COMPUTE_1));
    };
    p_graph.add_task(std::move(task_desc));
}

void PixelSort::add_compute_task(RendGraphDescVK& p_graph,
    std::string_view p_task_id,
    const Ref<ROComputePipelineVK>& p_pipeline,
    VkExtent3D Layout::* p_group_count,
    std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) const {
    add_compute_task(p_graph, p_task_id, DirectionPipelines{p_pipeline, p_pipeline}, p_group_count, p_aliases);
}

void PixelSort::add_indirect_compute_task(RendGraphDescVK& p_graph,
    std::string_view p_task_id,
    const Ref<ROComputePipelineVK>& p_pipeline,
    VkDeviceSize p_dispatch_offset,
    std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) const {
    RendTaskDescVK task_desc(get_allocator());
    task_desc.task_id = p_task_id;
    for (const auto& [task_resource_id, graph_resource_id] : p_aliases) {
        task_desc.alias(task_resource_id, graph_resource_id);
    }
    task_desc.factory = [this, p_pipeline, p_dispatch_offset](const RendGraphContextVK& /*p_context*/) {
        return make_toggleable_task(make_unique<RTRunComputePipelineIndirectVK>(get_allocator(),
            renderer->get_base_render_data(),
            p_pipeline,
            REND_RES_PIXEL_SORT_RADIX_SORT_DISPATCH,
            p_dispatch_offset,
            QueueFamilyVK::COMPUTE_1));
    };
    p_graph.add_task(std::move(task_desc));
}

} // namespace WhiteBirdEngine
