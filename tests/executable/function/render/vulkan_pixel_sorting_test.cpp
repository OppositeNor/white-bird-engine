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
#include "core/allocator/i_allocator.hh"
#include "core/async/worker.hh"
#include "core/engine_core.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/profiling/profiler.hh"
#include "function/application/application_game.hh"
#include "function/function_utils.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_targets_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_indirect_vk.hh"
#include "function/render_vk/rt_run_compute_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "glm/gtc/packing.hpp"
#include "global/global.hh"
#include "imgui.h"
#include "imgui_overlay_vk.hh"
#include "input_manager_mocked.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_utils.hh"
#include "rt_imgui_vk.hh"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <future>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WBE = WhiteBirdEngine;

inline constexpr std::string_view RT_COUNT_PIXEL_SORT_MASK = "count_pixel_sort_mask";
inline constexpr std::string_view RT_PREFIX_SUM_PIXEL_SORT_COLUMNS = "prefix_sum_pixel_sort_columns";
inline constexpr std::string_view RT_REGISTER_PIXEL_SORT_KEYS = "register_pixel_sort_keys";
inline constexpr std::string_view RT_RADIX_SORT_PREPARE_DISPATCH = "radix_sort_prepare_dispatch";
inline constexpr std::string_view RT_WRITE_SORTED_PIXELS = "write_sorted_pixels";
inline constexpr std::string_view RT_PIXEL_SORTING_POST_PROCESSING = "pixel_sorting_post_processing";
inline constexpr std::string_view RT_PRESENT_SORTED_PIXELS = "present_sorted_pixels";
inline constexpr std::string_view RT_DRAW_IMGUI = "draw_imgui";
inline constexpr std::string_view REND_RES_PIXEL_SORT_SOURCE = "pixel_sort_source";
inline constexpr std::string_view REND_RES_PIXEL_SORT_DESTINATION_IMAGE = "pixel_sort_destination_image";
inline constexpr std::string_view REND_RES_PIXEL_SORT_DESTINATION = "pixel_sort_destination";
inline constexpr std::string_view REND_RES_PIXEL_SORTING_POST_PROCESSING_OUTPUT_IMAGE = "pixel_sorting_post_processing_output_image";
inline constexpr std::string_view REND_RES_PIXEL_SORTING_POST_PROCESSING_OUTPUT = "pixel_sorting_post_processing_output";
inline constexpr std::string_view REND_RES_PIXEL_SORTING_POST_PROCESSING_PARAMS = "pixel_sorting_post_processing_params";
inline constexpr std::string_view REND_RES_PIXEL_SORT_SAMPLER = "pixel_sort_sampler";
inline constexpr std::string_view REND_RES_PIXEL_SORT_CONTRAST = "pixel_sort_contrast";
inline constexpr std::string_view REND_RES_PIXEL_SORT_COLUMN_OFFSETS = "pixel_sort_column_offsets";
inline constexpr std::string_view REND_RES_PIXEL_SORT_MASK_IMAGE = "pixel_sort_mask_image";
inline constexpr std::string_view REND_RES_PIXEL_SORT_MASK = "pixel_sort_mask";
// The register pass writes and the write pass reads this buffer; the radix sort passes alternate between it and the swap buffer.
inline constexpr std::string_view REND_RES_PIXEL_SORT_KEYS = "pixel_sort_keys";
inline constexpr std::string_view REND_RES_PIXEL_SORT_KEYS_SWAP = "pixel_sort_keys_swap";
inline constexpr std::string_view REND_RES_PIXEL_SORT_HISTOGRAM = "pixel_sort_histogram";
// The indirect dispatches of the radix sort passes, written from the local sort count of the registered keys.
inline constexpr std::string_view REND_RES_RADIX_SORT_DISPATCH = "radix_sort_dispatch";
inline constexpr VkFormat PIXEL_SORT_DESTINATION_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
// Must match the numthreads of write_sorted_pixels.slang and pixel_sorting_post_processing.slang.
inline constexpr uint32_t PIXEL_SORT_TILE_SIZE = 8;
// Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation per
// column.
inline constexpr uint32_t PIXEL_SORT_COLUMN_WORKGROUP_SIZE = 64;
// Must match WBE_PIXEL_SORT_COLUMN_OFFSETS_START in pixel_sort_utils.slang.
inline constexpr uint32_t PIXEL_SORT_COLUMN_OFFSETS_START = 4;
inline constexpr float PIXEL_SORT_DEFAULT_MIN_METRIC = 0.20F;
inline constexpr float PIXEL_SORT_DEFAULT_MAX_METRIC = 0.8F;
// Must match WBE_PIXEL_SORT_METRIC_SATURATION in pixel_sort_utils.slang: 0 sorts by luminance, 1 by hue, 2 by saturation.
inline constexpr uint32_t PIXEL_SORT_DEFAULT_METRIC = 0;
inline constexpr float PIXEL_SORTING_POST_PROCESSING_DEFAULT_GAMMA = 1.0F;
// Bytes of one R16G16B16A16_SFLOAT pixel of the result image.
inline constexpr size_t PIXEL_SORT_RESULT_BYTES_PER_PIXEL = 8;
// Directory, relative to the working directory, that receives the screenshots.
inline constexpr std::string_view PIXEL_SORT_SCREENSHOT_DIRECTORY = "build/screenshots";

namespace {

bool is_srgb_format(VkFormat p_format) {
    return p_format == VK_FORMAT_R8G8B8A8_SRGB || p_format == VK_FORMAT_B8G8R8A8_SRGB;
}

// The sRGB transfer function, applied by the hardware when a linear color is written to an sRGB swap chain.
float encode_srgb(float p_linear) {
    if (p_linear <= 0.0031308F) {
        return p_linear * 12.92F;
    }
    return 1.055F * std::pow(p_linear, 1.0F / 2.4F) - 0.055F;
}

// Convert the R16G16B16A16_SFLOAT result image to opaque RGBA8, encoded as the swap chain displays it.
std::vector<uint8_t> convert_result_to_rgba8(std::span<const uint8_t> p_result_data, bool p_encode_srgb) {
    if (p_result_data.size_bytes() % PIXEL_SORT_RESULT_BYTES_PER_PIXEL != 0) {
        throw std::runtime_error("Failed to convert the pixel sorting screenshot: byte count is not pixel aligned.");
    }
    const size_t pixel_count = p_result_data.size_bytes() / PIXEL_SORT_RESULT_BYTES_PER_PIXEL;
    std::vector<uint8_t> rgba8(pixel_count * 4);
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
        for (size_t channel = 0; channel < 3; ++channel) {
            uint16_t packed_channel = 0;
            std::memcpy(&packed_channel,
                p_result_data.data() + pixel * PIXEL_SORT_RESULT_BYTES_PER_PIXEL + channel * sizeof(packed_channel),
                sizeof(packed_channel));
            float value = glm::unpackHalf1x16(packed_channel);
            if (!std::isfinite(value)) {
                value = value > 0.0F ? 1.0F : 0.0F;
            }
            value = std::clamp(value, 0.0F, 1.0F);
            if (p_encode_srgb) {
                value = encode_srgb(value);
            }
            rgba8[pixel * 4 + channel] = static_cast<uint8_t>(std::lround(value * 255.0F));
        }
        rgba8[pixel * 4 + 3] = UINT8_MAX;
    }
    return rgba8;
}

// A screenshot path named after the current local time, e.g. build/screenshots/pixel_sorting_20261007_183012.png.
WBE::Path make_screenshot_path() {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const std::chrono::zoned_time local_time{std::chrono::current_zone(), now};
    return WBE::Path(PIXEL_SORT_SCREENSHOT_DIRECTORY) / std::format("pixel_sorting_{:%Y%m%d_%H%M%S}.png", local_time);
}

} // namespace
// Must match WBE_RADIX_SORT_BUCKET_COUNT, WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP, WBE_RADIX_SORT_MAX_WORKGROUP_COUNT,
// WBE_RADIX_SORT_DATA_START,
// WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM, WBE_RADIX_SORT_LOCAL_SORT_DISPATCH_START, and
// WBE_RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_START in radix_sort_utils.slang.
inline constexpr uint32_t RADIX_SORT_BUCKET_COUNT = 256;
inline constexpr uint32_t RADIX_SORT_ELEMENTS_PER_WORKGROUP = 256;
inline constexpr uint32_t RADIX_SORT_MAX_WORKGROUP_COUNT = 16384;
inline constexpr uint32_t RADIX_SORT_DATA_START = 4;
inline constexpr uint32_t RADIX_SORT_SCAN_HISTOGRAM_NUM = 16;
inline constexpr VkDeviceSize RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET = 0;
inline constexpr VkDeviceSize RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_OFFSET = 3 * sizeof(uint32_t);
inline constexpr size_t RADIX_SORT_DISPATCH_BUFFER_SIZE = 2 * sizeof(VkDispatchIndirectCommand);
// Each pass sorts by one 8-bit digit of the 32-bit keys.
inline constexpr uint32_t RADIX_SORT_PASS_COUNT = 4;

/**
 * @enum PixelSortOrientation
 * @brief The lines of the image the pixel sort sorts.
 */
enum class PixelSortOrientation : uint8_t {
    /** Sort every column, from the top. */
    VERTICAL,
    /** Sort every row, from the left. */
    HORIZONTAL,
};

/**
 * @class PixelSortContrastGPU
 * @brief GPU layout of PixelSortContrast in pixel_sort_utils.slang.
 */
struct PixelSortContrastGPU {
    /** The smallest sort metric of a sorted pixel, in [0, 1]. */
    float min_metric = PIXEL_SORT_DEFAULT_MIN_METRIC;
    /** The largest sort metric of a sorted pixel, in [0, 1]. */
    float max_metric = PIXEL_SORT_DEFAULT_MAX_METRIC;
    /** The metric the sorted pixels are ordered and masked by: 0 for luminance, 1 for hue, 2 for saturation. */
    uint32_t sort_metric = PIXEL_SORT_DEFAULT_METRIC;
    /** The pixels each sort covers: 0 for the whole line, as the R16_UINT mask image requires, 1 for each span. */
    uint32_t sort_scope = 0;
};

/**
 * @class PixelSortingPostProcessingParamsGPU
 * @brief GPU layout of PixelSortingPostProcessingParams in pixel_sorting_post_processing.slang.
 */
struct PixelSortingPostProcessingParamsGPU {
    /** The exponent every color channel is raised to; 1 leaves the color unchanged. */
    float gamma = PIXEL_SORTING_POST_PROCESSING_DEFAULT_GAMMA;
};

/**
 * @class ApplicationVulkanPixelSortingTest
 * @brief Sorts the pixels of every column or row of an image that are inside a contrast mask with the GPU radix sort, and
 * presents the result.
 *
 * The orientation selects whether the columns or the rows are sorted; changing it rebuilds the render graph. One
 * invocation per line counts its masked pixels, a prefix sum turns the counts into the range of every line in the key
 * buffer, and one invocation per line registers its masked pixels into that range and stores each pixel's buffer index
 * in an index image. Every key orders by line, then metric, and stores the position along the line (see
 * pixel_sort_utils.slang), so one radix sort of all keys sorts every line in place within its range. A single invocation
 * writes the indirect dispatches of the radix sort from the number of registered keys. Each of the 4 radix sort passes
 * runs the histogram, histogram prefix scan, overflow prefix sum, overflow addition, and scatter shaders for one 8-bit
 * digit, alternating between two key buffers. Then, every masked pixel reads the sorted key at its stored index, and the other pixels keep their color.
 * Finally, a post-processing pass applies general effects to the sorted image before it is presented.
 */
class ApplicationVulkanPixelSortingTest : public WBE::ApplicationGame {
public:
    ApplicationVulkanPixelSortingTest()
        : WBE::ApplicationGame(
              WBE::RendererCapabilities{
                  .timestamp_query = WBE::exec_test_requested("WBE_EXEC_TEST_TIMESTAMP_QUERY"),
                  .descriptor_set = true,
              },
              WBE::make_ref<WBE::InputManagerMocked>(WBE::global_allocator(), WBE::global_allocator())) {
    }

    virtual void logic_update(double p_delta) override {
    }

    virtual void render_update(double p_delta) override {
        auto* renderer = WBE::RendererVK::get_singleton();
        WBE::on_exec_test_render(*renderer, renderer->get_base_render_data()->current_frame);
        poll_screenshot();
        if (ui_orientation != built_orientation) {
            rebuild_render_graph();
        }
        imgui_overlay->begin_frame(p_delta);
        draw_parameter_window();
        imgui_overlay->end_frame();
    }

    /**
     * @brief Set the sort metric range of the sorted pixels.
     *
     * Before setup_render_graph(), the range becomes the initial contrast buffer content; afterwards the contrast is
     * updated through a render job. Must be called from the logic thread.
     *
     * @param p_min_metric The smallest sort metric of a sorted pixel.
     * @param p_max_metric The largest sort metric of a sorted pixel.
     * @throws std::runtime_error If the range is not within [0, 1] or p_min_metric exceeds p_max_metric.
     */
    void set_contrast(float p_min_metric, float p_max_metric) {
        if (!(0.0F <= p_min_metric && p_min_metric <= p_max_metric && p_max_metric <= 1.0F)) {
            throw std::runtime_error(std::format(
                "Invalid pixel sort contrast range [{}, {}]: it must satisfy 0 <= min <= max <= 1.", p_min_metric, p_max_metric));
        }
        contrast.min_metric = p_min_metric;
        contrast.max_metric = p_max_metric;
        if (!render_graph_built) {
            return;
        }
        // The job resolves the current buffer, because a graph rebuild replaces it.
        add_render_job([this, p_min_metric, p_max_metric] {
            ui_contrast.min_metric = p_min_metric;
            ui_contrast.max_metric = p_max_metric;
            contrast_buffer->update_buffer(&ui_contrast, sizeof(PixelSortContrastGPU));
        });
    }

    /**
     * @brief Set the gamma the post-processing pass raises every color channel to.
     *
     * Before setup_render_graph(), the gamma becomes the initial parameter buffer content; afterwards the gamma is updated
     * through a render job. Must be called from the logic thread.
     *
     * @param p_gamma The exponent every color channel is raised to.
     * @throws std::runtime_error If p_gamma is not positive.
     */
    void set_gamma(float p_gamma) {
        if (!(p_gamma > 0.0F)) {
            throw std::runtime_error(std::format("Invalid pixel sorting post-processing gamma {}: it must be positive.", p_gamma));
        }
        post_processing_params.gamma = p_gamma;
        if (!render_graph_built) {
            return;
        }
        // The job resolves the current buffer, because a graph rebuild replaces it.
        add_render_job([this, p_gamma] {
            ui_post_processing_params.gamma = p_gamma;
            post_processing_params_buffer->update_buffer(&ui_post_processing_params, sizeof(PixelSortingPostProcessingParamsGPU));
        });
    }

    /**
     * @brief Set whether the columns or the rows of the image are sorted.
     *
     * Before setup_render_graph(), the orientation selects the initial render graph; afterwards the graph is rebuilt for it
     * through a render job. Must be called from the logic thread.
     *
     * @param p_orientation The lines of the image to sort.
     */
    void set_orientation(PixelSortOrientation p_orientation) {
        orientation = p_orientation;
        if (!render_graph_built) {
            return;
        }
        add_render_job([this, p_orientation] {
            ui_orientation = p_orientation;
        });
    }

    /**
     * @brief Load the source image, size the window to it, and build the render graph for the configured orientation.
     *
     * Must be called once, before run().
     *
     * @throws std::runtime_error If a resource is missing, the image is too large to sort, or the graph cannot be built.
     */
    void setup_render_graph() {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const auto* base_render_data = renderer->get_base_render_data();
        auto* resource_manager = WBE::ResourceManager::get_singleton();
        imgui_overlay = WBE::make_unique<WBE::ImGuiOverlayVK>(WBE::global_allocator(), renderer);

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
        source_view = graphics_compiler.compile_texture_view(REND_RES_PIXEL_SORT_SOURCE, texture_resource);
        present_pipeline = graphics_compiler.compile("present_pixel_sort_pipeline", present_pipeline_resource).graphics_pipeline;

        const VkExtent3D source_extent = source_view->fields().image->fields().extent;
        const uint64_t element_count = static_cast<uint64_t>(source_extent.width) * source_extent.height;
        // A key packs the line, metric, and position along the line, so at least 2 metric levels must fit in 32 bits.
        if (element_count == 0 || UINT32_MAX / element_count < 2) {
            throw std::runtime_error(std::format("Failed to setup pixel sorting test: a {}x{} image is too large to pack its pixel sort keys into 32 bits.",
                source_extent.width,
                source_extent.height));
        }
        // Show the image at its own size. This runs before the logic and rendering threads start, and the renderer
        // resizes the swap chain, and with it the graph, on the first frame.
        get_window()->resize(source_extent.width, source_extent.height);
        ui_contrast = contrast;
        ui_post_processing_params = post_processing_params;
        ui_orientation = orientation;
        build_render_graph(orientation);
        render_graph_built = true;
    }

private:
    static constexpr std::string_view PIXEL_SORTING_GRAPH_ID = "pixel_sorting_graph";

    /**
     * @brief Replace the render graph with one for the orientation selected on the rendering thread.
     *
     * Must be called on the rendering thread, outside of the renderer tick; the renderer releases the previous graph once
     * its frames have finished.
     */
    void rebuild_render_graph() {
        WBE::RendererVK::get_singleton()->unregister_render_graph(PIXEL_SORTING_GRAPH_ID);
        build_render_graph(ui_orientation);
    }

    /**
     * @brief Build, register, and activate the render graph that sorts the lines of p_orientation.
     *
     * The contrast and post-processing buffers start with the parameters of the rendering thread.
     *
     * @param p_orientation The lines of the image to sort.
     * @throws std::runtime_error If a sorted line is too long for the 16-bit pixel sort mask, or a shader is missing.
     */
    void build_render_graph(PixelSortOrientation p_orientation) {
        auto* render_allocator = get_renderer_allocator().get();
        auto* renderer = WBE::RendererVK::get_singleton();
        const PixelSortGraphLayout layout = make_graph_layout(p_orientation);
        WBE::RendGraphDescVK pipeline_desc(render_allocator);
        add_graph_resources(pipeline_desc, layout);
        const RadixSortTaskIds radix_sort_task_ids = add_sort_tasks(pipeline_desc, layout);
        add_present_tasks(pipeline_desc);
        connect_graph_tasks(pipeline_desc, radix_sort_task_ids);
        WBE::Ref<WBE::RenderGraphVK> graph = renderer->register_render_graph(PIXEL_SORTING_GRAPH_ID, std::move(pipeline_desc));
        contrast_buffer = graph->acquire_resource(REND_RES_PIXEL_SORT_CONTRAST)->get().dynamic_cast_ref<WBE::ROBufferVK>();
        post_processing_params_buffer =
            graph->acquire_resource(REND_RES_PIXEL_SORTING_POST_PROCESSING_PARAMS)->get().dynamic_cast_ref<WBE::ROBufferVK>();
        result_image = graph->acquire_resource(REND_RES_PIXEL_SORTING_POST_PROCESSING_OUTPUT_IMAGE);
        WBE::write_descriptor_debug_output("pixel_sorting_graph_descriptors", graph->list_descriptors());
        renderer->activate_render_graph(PIXEL_SORTING_GRAPH_ID);
        built_orientation = p_orientation;
    }

    // The sizes of the resources and dispatches of a graph for the source image and one orientation.
    struct PixelSortGraphLayout {
        VkExtent3D source_extent{};
        std::string_view orientation_name;
        uint32_t column_group_count = 0;
        VkExtent3D tile_group_count{};
        size_t column_offsets_buffer_size = 0;
        size_t key_buffer_size = 0;
        size_t histogram_buffer_size = 0;
    };

    // The ids of the histogram, histogram prefix sum, overflow prefix sum, overflow addition, and scatter tasks of every
    // radix sort pass.
    struct RadixSortTaskIds {
        std::array<std::string, RADIX_SORT_PASS_COUNT> histogram_task_ids;
        std::array<std::string, RADIX_SORT_PASS_COUNT> prefix_sum_task_ids;
        std::array<std::string, RADIX_SORT_PASS_COUNT> overflow_prefix_sum_task_ids;
        std::array<std::string, RADIX_SORT_PASS_COUNT> add_overflow_task_ids;
        std::array<std::string, RADIX_SORT_PASS_COUNT> scatter_task_ids;
    };

    PixelSortGraphLayout make_graph_layout(PixelSortOrientation p_orientation) const {
        PixelSortGraphLayout layout;
        layout.source_extent = source_view->fields().image->fields().extent;
        const VkExtent3D source_extent = layout.source_extent;
        const uint64_t element_count = static_cast<uint64_t>(source_extent.width) * source_extent.height;
        const bool vertical = p_orientation == PixelSortOrientation::VERTICAL;
        layout.orientation_name = vertical ? "vertical" : "horizontal";
        // The number of sorted lines, and the number of pixels in each.
        const uint32_t line_count = vertical ? source_extent.width : source_extent.height;
        const uint32_t line_length = vertical ? source_extent.height : source_extent.width;
        // The mask image stores each masked pixel's rank within its line as R16_UINT, with 0xFFFF marking unmasked pixels.
        if (line_length >= UINT16_MAX) {
            throw std::runtime_error(std::format(
                "Failed to build the pixel sorting graph: a {}-pixel line is too long for the 16-bit pixel sort mask.", line_length));
        }
        // The radix sort has one local sort per RADIX_SORT_ELEMENTS_PER_WORKGROUP keys, up to RADIX_SORT_MAX_WORKGROUP_COUNT, and
        // sizes its buffers for the case where every pixel is inside the mask.
        const uint64_t max_local_sort_num = std::clamp<uint64_t>(
            (element_count + RADIX_SORT_ELEMENTS_PER_WORKGROUP - 1) / RADIX_SORT_ELEMENTS_PER_WORKGROUP, 1, RADIX_SORT_MAX_WORKGROUP_COUNT);
        const uint64_t max_overflow_num = (max_local_sort_num + RADIX_SORT_SCAN_HISTOGRAM_NUM - 1) / RADIX_SORT_SCAN_HISTOGRAM_NUM;
        layout.tile_group_count = {
            .width = (source_extent.width + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
            .height = (source_extent.height + PIXEL_SORT_TILE_SIZE - 1) / PIXEL_SORT_TILE_SIZE,
            .depth = 1,
        };
        layout.column_group_count = (line_count + PIXEL_SORT_COLUMN_WORKGROUP_SIZE - 1) / PIXEL_SORT_COLUMN_WORKGROUP_SIZE;
        layout.column_offsets_buffer_size = (PIXEL_SORT_COLUMN_OFFSETS_START + line_count) * sizeof(uint32_t);
        // Sized for the case where every pixel is inside the mask.
        layout.key_buffer_size = (RADIX_SORT_DATA_START + element_count) * sizeof(uint32_t);
        // The metadata, one sub-histogram per local sort, and one overflow histogram per histogram prefix scan workgroup.
        layout.histogram_buffer_size =
            (RADIX_SORT_DATA_START + (max_local_sort_num + max_overflow_num) * RADIX_SORT_BUCKET_COUNT) * sizeof(uint32_t);
        return layout;
    }

    // The contrast and post-processing buffers start with the parameters of the rendering thread.
    void add_graph_resources(WBE::RendGraphDescVK& p_graph, const PixelSortGraphLayout& p_layout) const {
        p_graph.add_resource(WBE::REND_RES_SWAP_CHAIN, *WBE::RendererVK::get_singleton()->acquire_swap_chain_image_view_container())
            .add_resource(REND_RES_PIXEL_SORT_SOURCE, *source_view)
            .add_buffer(REND_RES_PIXEL_SORT_CONTRAST,
                // The initial contrast is captured once, so the buffer keeps its identity, and the updates written to it,
                // when the factory runs again.
                [initial_data = WBE::to_initial_data(ui_contrast)](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendBufferDescVK{
                        .usage = WBE::RendResPrimaryUsageVK::BUFFER_UNIFORM,
                        .domain = WBE::DeviceDomain::HOST_DEVICE,
                        .initial_data = initial_data,
                    };
                })
            .add_buffer(REND_RES_PIXEL_SORT_COLUMN_OFFSETS,
                [size = p_layout.column_offsets_buffer_size](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendBufferDescVK{.usage = WBE::RendResPrimaryUsageVK::BUFFER_STORAGE, .size = size};
                })
            .add_buffer(REND_RES_PIXEL_SORT_KEYS,
                [size = p_layout.key_buffer_size](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendBufferDescVK{.usage = WBE::RendResPrimaryUsageVK::BUFFER_STORAGE, .size = size};
                })
            .add_buffer(REND_RES_PIXEL_SORT_KEYS_SWAP,
                [size = p_layout.key_buffer_size](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendBufferDescVK{.usage = WBE::RendResPrimaryUsageVK::BUFFER_STORAGE, .size = size};
                })
            .add_buffer(REND_RES_PIXEL_SORT_HISTOGRAM,
                [size = p_layout.histogram_buffer_size](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendBufferDescVK{.usage = WBE::RendResPrimaryUsageVK::BUFFER_STORAGE, .size = size};
                })
            .add_buffer(REND_RES_RADIX_SORT_DISPATCH,
                [](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendBufferDescVK{
                        .usage = WBE::RendResPrimaryUsageVK::BUFFER_INDIRECT, .size = RADIX_SORT_DISPATCH_BUFFER_SIZE};
                })
            .add_image(REND_RES_PIXEL_SORT_DESTINATION_IMAGE,
                [extent = p_layout.source_extent](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendImageDescVK{
                        .usage = WBE::RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                        .format = PIXEL_SORT_DESTINATION_FORMAT,
                        .extent = {.width = extent.width, .height = extent.height, .depth = 1},
                    };
                })
            .add_image_view(REND_RES_PIXEL_SORT_DESTINATION, REND_RES_PIXEL_SORT_DESTINATION_IMAGE)
            .add_buffer(REND_RES_PIXEL_SORTING_POST_PROCESSING_PARAMS,
                [initial_data = WBE::to_initial_data(ui_post_processing_params)](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendBufferDescVK{
                        .usage = WBE::RendResPrimaryUsageVK::BUFFER_UNIFORM,
                        .domain = WBE::DeviceDomain::HOST_DEVICE,
                        .initial_data = initial_data,
                    };
                })
            .add_image(REND_RES_PIXEL_SORTING_POST_PROCESSING_OUTPUT_IMAGE,
                [extent = p_layout.source_extent](const WBE::RendGraphContextVK& /*p_context*/) {
                    // The screenshot reads the result back through a transfer.
                    return WBE::RendImageDescVK{
                        .usage = WBE::RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                        .additional_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        .format = PIXEL_SORT_DESTINATION_FORMAT,
                        .extent = {.width = extent.width, .height = extent.height, .depth = 1},
                    };
                })
            .add_image_view(REND_RES_PIXEL_SORTING_POST_PROCESSING_OUTPUT, REND_RES_PIXEL_SORTING_POST_PROCESSING_OUTPUT_IMAGE)
            .add_image(REND_RES_PIXEL_SORT_MASK_IMAGE,
                [extent = p_layout.source_extent](const WBE::RendGraphContextVK& /*p_context*/) {
                    return WBE::RendImageDescVK{
                        .usage = WBE::RendResPrimaryUsageVK::IMAGE_READ_WRITE,
                        .format = VK_FORMAT_R16_UINT,
                        .extent = {.width = extent.width, .height = extent.height, .depth = 1},
                    };
                })
            .add_image_view(REND_RES_PIXEL_SORT_MASK, REND_RES_PIXEL_SORT_MASK_IMAGE)
            .add_sampler(REND_RES_PIXEL_SORT_SAMPLER, [=](const WBE::RendGraphContextVK& /*p_context*/) {
                return WBE::RendSamplerDescVK{
                    .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                };
            });
    }

    RadixSortTaskIds add_sort_tasks(WBE::RendGraphDescVK& p_graph, const PixelSortGraphLayout& p_layout) {
        const VkExtent3D column_dispatch{.width = p_layout.column_group_count, .height = 1, .depth = 1};
        add_compute_task(p_graph,
            RT_COUNT_PIXEL_SORT_MASK,
            std::format("count_pixel_sort_mask_{}_shader", p_layout.orientation_name),
            column_dispatch,
            {});
        // The prefix sum runs in a single workgroup.
        add_compute_task(p_graph,
            RT_PREFIX_SUM_PIXEL_SORT_COLUMNS,
            "prefix_sum_pixel_sort_columns_shader",
            {.width = 1, .height = 1, .depth = 1},
            {});
        add_compute_task(p_graph,
            RT_REGISTER_PIXEL_SORT_KEYS,
            std::format("register_pixel_sort_keys_{}_shader", p_layout.orientation_name),
            column_dispatch,
            {});

        RadixSortTaskIds task_ids = add_radix_sort_tasks(p_graph);

        // After an even number of passes, the sorted keys are back in the key buffer.
        add_compute_task(p_graph,
            RT_WRITE_SORTED_PIXELS,
            std::format("write_sorted_pixels_{}_shader", p_layout.orientation_name),
            p_layout.tile_group_count,
            {});
        add_compute_task(p_graph,
            RT_PIXEL_SORTING_POST_PROCESSING,
            "pixel_sorting_post_processing_shader",
            p_layout.tile_group_count,
            {});
        return task_ids;
    }

    RadixSortTaskIds add_radix_sort_tasks(WBE::RendGraphDescVK& p_graph) {
        RadixSortTaskIds task_ids;
        // The element count is fixed once the keys are registered, so every pass uses the same dispatches.
        add_compute_task(p_graph,
            RT_RADIX_SORT_PREPARE_DISPATCH,
            "radix_sort_prepare_dispatch",
            {.width = 1, .height = 1, .depth = 1},
            {{"input_buffer", REND_RES_PIXEL_SORT_KEYS}, {"dispatch_buffer", REND_RES_RADIX_SORT_DISPATCH}});
        for (uint32_t pass = 0; pass < RADIX_SORT_PASS_COUNT; ++pass) {
            const std::string_view input_keys = pass % 2 == 0 ? REND_RES_PIXEL_SORT_KEYS : REND_RES_PIXEL_SORT_KEYS_SWAP;
            const std::string_view output_keys = pass % 2 == 0 ? REND_RES_PIXEL_SORT_KEYS_SWAP : REND_RES_PIXEL_SORT_KEYS;
            const std::string bit_region = std::format("{}_{}", pass * 8, (pass + 1) * 8);
            task_ids.histogram_task_ids[pass] = std::format("radix_sort_histogram_{}", bit_region);
            task_ids.prefix_sum_task_ids[pass] = std::format("radix_sort_histogram_prefix_sum_{}", bit_region);
            task_ids.overflow_prefix_sum_task_ids[pass] = std::format("radix_sort_histogram_overflow_prefix_sum_{}", bit_region);
            task_ids.add_overflow_task_ids[pass] = std::format("radix_sort_histogram_add_overflow_{}", bit_region);
            task_ids.scatter_task_ids[pass] = std::format("radix_sort_scatter_{}", bit_region);
            add_indirect_compute_task(p_graph,
                task_ids.histogram_task_ids[pass],
                std::format("radix_sort_histogram_workgroup_process_{}", bit_region),
                RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
                {{"input_buffer", input_keys}, {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
            add_indirect_compute_task(p_graph,
                task_ids.prefix_sum_task_ids[pass],
                "radix_sort_histogram_prefix_sum",
                RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_OFFSET,
                {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
            // The overflow histograms are scanned in a single workgroup.
            add_compute_task(p_graph,
                task_ids.overflow_prefix_sum_task_ids[pass],
                "radix_sort_histogram_overflow_prefix_sum",
                {.width = 1, .height = 1, .depth = 1},
                {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
            add_indirect_compute_task(p_graph,
                task_ids.add_overflow_task_ids[pass],
                "radix_sort_histogram_add_overflow",
                RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
                {{"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM}});
            add_indirect_compute_task(p_graph,
                task_ids.scatter_task_ids[pass],
                std::format("radix_sort_scatter_{}", bit_region),
                RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
                {
                    {"input_buffer", input_keys},
                    {"histogram_buffer", REND_RES_PIXEL_SORT_HISTOGRAM},
                    {"output_buffer", output_keys},
                });
        }
        return task_ids;
    }

    void add_present_tasks(WBE::RendGraphDescVK& p_graph) {
        auto* render_allocator = get_renderer_allocator().get();
        const auto* base_render_data = WBE::RendererVK::get_singleton()->get_base_render_data();
        WBE::RendTaskDescVK present_task_desc(render_allocator);
        present_task_desc.task_id = RT_PRESENT_SORTED_PIXELS;
        present_task_desc.alias(WBE::RenderTargetsVK::color_id(0), WBE::REND_RES_SWAP_CHAIN);
        const std::array<VkClearValue, 1> clear_values{VkClearValue{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}}};
        present_task_desc.factory = [render_allocator, base_render_data, pipeline = present_pipeline, clear_values](
                                        const WBE::RendGraphContextVK& p_context) {
            return WBE::RTRunGraphicsPipelineVK::Builder(render_allocator)
                .set_pipeline(pipeline)
                .set_render_extent(p_context.swap_chain_extent)
                .set_clear_values(clear_values)
                .set_draw_vertex_count(3)
                .create_unique(base_render_data);
        };
        p_graph.add_task(std::move(present_task_desc));

        WBE::RendTaskDescVK imgui_task_desc(render_allocator);
        imgui_task_desc.task_id = RT_DRAW_IMGUI;
        imgui_task_desc.alias(WBE::RenderTargetsVK::color_id(0), WBE::REND_RES_SWAP_CHAIN);
        imgui_task_desc.factory = [this, render_allocator, base_render_data](const WBE::RendGraphContextVK& /*p_context*/) {
            return WBE::make_unique<WBE::RTImGuiVK>(render_allocator, base_render_data, imgui_overlay.get());
        };
        p_graph.add_task(std::move(imgui_task_desc));
    }

    static void connect_graph_tasks(WBE::RendGraphDescVK& p_graph, const RadixSortTaskIds& p_task_ids) {
        WBE::connect_resource_chain(p_graph,
            REND_RES_PIXEL_SORT_COLUMN_OFFSETS,
            {RT_COUNT_PIXEL_SORT_MASK, RT_PREFIX_SUM_PIXEL_SORT_COLUMNS, RT_REGISTER_PIXEL_SORT_KEYS, RT_WRITE_SORTED_PIXELS});
        WBE::connect_resource_chain(p_graph, REND_RES_PIXEL_SORT_MASK, {RT_REGISTER_PIXEL_SORT_KEYS, RT_WRITE_SORTED_PIXELS});
        // Every task using a key buffer or the histogram, in execution order. Pass p reads the keys from one buffer and
        // scatters them into the other, so each buffer is written by every other scatter.
        WBE::connect_resource_chain(p_graph,
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
        WBE::connect_resource_chain(p_graph,
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
        WBE::connect_resource_chain(p_graph, REND_RES_PIXEL_SORT_HISTOGRAM, histogram_chain);
        WBE::connect_resource_chain(p_graph, REND_RES_RADIX_SORT_DISPATCH, dispatch_chain);
        WBE::connect_resource_chain(
            p_graph, REND_RES_PIXEL_SORT_DESTINATION, {RT_WRITE_SORTED_PIXELS, RT_PIXEL_SORTING_POST_PROCESSING});
        WBE::connect_resource_chain(
            p_graph, REND_RES_PIXEL_SORTING_POST_PROCESSING_OUTPUT, {RT_PIXEL_SORTING_POST_PROCESSING, RT_PRESENT_SORTED_PIXELS});
        WBE::connect_resource_chain(p_graph, WBE::REND_RES_SWAP_CHAIN, {RT_PRESENT_SORTED_PIXELS, RT_DRAW_IMGUI});
    }

    // The parameters configured on the logic thread before setup_render_graph().
    PixelSortContrastGPU contrast;
    PixelSortOrientation orientation = PixelSortOrientation::VERTICAL;
    // Whether setup_render_graph() has run; afterwards the configuration is forwarded to the rendering thread.
    bool render_graph_built = false;
    // The source image and the present pipeline, shared by every graph built for an orientation.
    WBE::Ref<WBE::ROImageViewVK> source_view;
    WBE::Ref<WBE::ROGraphicsPipelineVK> present_pipeline;
    // The orientation selected in the parameter window, and the one the current graph sorts; owned by the rendering
    // thread.
    PixelSortOrientation ui_orientation = PixelSortOrientation::VERTICAL;
    PixelSortOrientation built_orientation = PixelSortOrientation::VERTICAL;
    // The contrast edited in the parameter window; owned by the rendering thread.
    PixelSortContrastGPU ui_contrast;
    // Contrast buffer of the current graph, updated by the parameter window and by render jobs of the logic thread.
    WBE::Ref<WBE::ROBufferVK> contrast_buffer;
    PixelSortingPostProcessingParamsGPU post_processing_params;
    // The post-processing parameters edited in the parameter window; owned by the rendering thread.
    PixelSortingPostProcessingParamsGPU ui_post_processing_params;
    // Post-processing parameter buffer of the current graph, updated by the parameter window and by render jobs of the
    // logic thread.
    WBE::Ref<WBE::ROBufferVK> post_processing_params_buffer;
    // Destroyed before the renderer, after the rendering thread has stopped.
    WBE::Unique<WBE::ImGuiOverlayVK> imgui_overlay;
    // Graph-owned container of the post-processed result image, the content of the screenshots.
    WBE::Ref<WBE::RenderObjectContainerVK> result_image;
    // The screenshot in progress, owned by the rendering thread: first the readback of the result image, then the worker
    // task that converts and writes it.
    std::optional<std::future<std::vector<uint8_t>>> screenshot_readback;
    std::future<void> screenshot_write;
    VkExtent2D screenshot_extent{};

    bool is_screenshot_in_progress() const {
        return screenshot_readback.has_value() || screenshot_write.valid();
    }

    /**
     * @brief Start reading back the result image for a screenshot. Must be called on the rendering thread.
     *
     * The result image is the post-processed image before the present and UI passes, so the UI is not in the screenshot.
     * The readback copies the result of the previous frame at the start of the next one.
     */
    void request_screenshot() {
        WBE::Ref<WBE::ROImageVK> image = result_image->get().dynamic_cast_ref<WBE::ROImageVK>();
        if (image == WBE::MEM_NULL) {
            throw std::runtime_error("Failed to take a pixel sorting screenshot: the result image is not an image.");
        }
        const VkExtent3D extent = image->fields().extent;
        screenshot_extent = {.width = extent.width, .height = extent.height};
        screenshot_readback = image->acquire_image_content();
    }

    /**
     * @brief Advance the screenshot in progress: hand a completed readback to a worker, and collect a completed write.
     *
     * Must be called on the rendering thread.
     *
     * @throws std::runtime_error If the readback, conversion, or PNG write failed.
     */
    void poll_screenshot() {
        constexpr std::chrono::seconds NO_WAIT{0};
        if (screenshot_readback.has_value() && screenshot_readback->wait_for(NO_WAIT) == std::future_status::ready) {
            std::vector<uint8_t> result_data = screenshot_readback->get();
            screenshot_readback.reset();
            const bool encode_srgb = is_srgb_format(WBE::RendererVK::get_singleton()->get_base_render_data()->swap_chain_format);
            // Converting and encoding a full-size image takes long enough to stall a frame, so a worker does it.
            screenshot_write = WBE::Worker::get_singleton()->submit_task(
                [result_data = std::move(result_data), extent = screenshot_extent, encode_srgb, path = make_screenshot_path()] {
                    const std::vector<uint8_t> rgba8 = convert_result_to_rgba8(result_data, encode_srgb);
                    std::filesystem::create_directories(path.parent_path());
                    WBE::write_image_data_png(path, rgba8, extent.width, extent.height, 4);
                    WBE::stdout_log(WBE::WBE_CHANNEL_DEBUG)->message(std::format("Saved pixel sorting screenshot to {}.", path.string()));
                });
        }
        if (screenshot_write.valid() && screenshot_write.wait_for(NO_WAIT) == std::future_status::ready) {
            screenshot_write.get();
        }
    }

    /**
     * @brief Draw the parameter window and upload the contrast and post-processing parameters when they change. Must be called on the rendering thread.
     */
    void draw_parameter_window() {
        static constexpr std::array<const char*, 3> SORT_METRIC_NAMES{"Luminance", "Hue", "Saturation"};
        bool changed = false;
        ImGui::Begin("Pixel Sorting", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        changed |= ImGui::SliderFloat("Min metric", &ui_contrast.min_metric, 0.0F, 1.0F);
        changed |= ImGui::SliderFloat("Max metric", &ui_contrast.max_metric, 0.0F, 1.0F);
        int sort_metric = static_cast<int>(ui_contrast.sort_metric);
        // Switching rebuilds the graph, which would release the result image of a screenshot in progress.
        static constexpr std::array<const char*, 2> ORIENTATION_NAMES{"Vertical", "Horizontal"};
        int selected_orientation = static_cast<int>(ui_orientation);
        ImGui::BeginDisabled(is_screenshot_in_progress());
        if (ImGui::Combo("Orientation", &selected_orientation, ORIENTATION_NAMES.data(), static_cast<int>(ORIENTATION_NAMES.size()))) {
            ui_orientation = static_cast<PixelSortOrientation>(selected_orientation);
        }
        ImGui::EndDisabled();
        if (ImGui::Combo("Sort metric", &sort_metric, SORT_METRIC_NAMES.data(), static_cast<int>(SORT_METRIC_NAMES.size()))) {
            ui_contrast.sort_metric = static_cast<uint32_t>(sort_metric);
            changed = true;
        }
        if (ImGui::SliderFloat("Gamma", &ui_post_processing_params.gamma, 0.1F, 4.0F)) {
            ui_post_processing_params.gamma = std::max(ui_post_processing_params.gamma, 0.1F);
            post_processing_params_buffer->update_buffer(&ui_post_processing_params, sizeof(PixelSortingPostProcessingParamsGPU));
        }
        const bool screenshot_in_progress = is_screenshot_in_progress();
        ImGui::BeginDisabled(screenshot_in_progress);
        if (ImGui::Button("Screenshot")) {
            request_screenshot();
        }
        ImGui::EndDisabled();
        if (screenshot_in_progress) {
            ImGui::SameLine();
            ImGui::TextUnformatted("Saving...");
        }
        ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0F / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
        if (!changed) {
            return;
        }
        ui_contrast.max_metric = std::max(ui_contrast.min_metric, ui_contrast.max_metric);
        contrast_buffer->update_buffer(&ui_contrast, sizeof(PixelSortContrastGPU));
    }

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
        const auto* base_render_data = WBE::RendererVK::get_singleton()->get_base_render_data();
        WBE::Ref<WBE::ROComputePipelineVK> pipeline = compile_compute_pipeline(p_shader_id);
        WBE::RendTaskDescVK task_desc(render_allocator);
        task_desc.task_id = p_task_id;
        for (const auto& [task_resource_id, graph_resource_id] : p_aliases) {
            task_desc.alias(task_resource_id, graph_resource_id);
        }
        task_desc.factory = [=](const WBE::RendGraphContextVK& /*p_context*/) {
            return WBE::make_unique<WBE::RTRunComputePipelineVK>(render_allocator, base_render_data, pipeline, p_dispatch_group_count);
        };
        p_graph.add_task(std::move(task_desc));
    }

    /**
     * @brief Add a task that dispatches a compute shader resource with the radix sort dispatch at an offset of the radix sort
     * dispatch buffer.
     * @param p_graph The graph descriptor receiving the task.
     * @param p_task_id The task id.
     * @param p_shader_id The resource id of the compute shader.
     * @param p_dispatch_offset The byte offset of the VkDispatchIndirectCommand in the radix sort dispatch buffer.
     * @param p_aliases Pairs of a shader resource id and the graph resource it refers to.
     * @throws std::runtime_error If the shader resource is not found.
     */
    void add_indirect_compute_task(WBE::RendGraphDescVK& p_graph,
        std::string_view p_task_id,
        std::string_view p_shader_id,
        VkDeviceSize p_dispatch_offset,
        std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) {
        auto* render_allocator = get_renderer_allocator().get();
        const auto* base_render_data = WBE::RendererVK::get_singleton()->get_base_render_data();
        WBE::Ref<WBE::ROComputePipelineVK> pipeline = compile_compute_pipeline(p_shader_id);
        WBE::RendTaskDescVK task_desc(render_allocator);
        task_desc.task_id = p_task_id;
        for (const auto& [task_resource_id, graph_resource_id] : p_aliases) {
            task_desc.alias(task_resource_id, graph_resource_id);
        }
        task_desc.factory = [=](const WBE::RendGraphContextVK& /*p_context*/) {
            return WBE::make_unique<WBE::RTRunComputePipelineIndirectVK>(
                render_allocator, base_render_data, pipeline, REND_RES_RADIX_SORT_DISPATCH, p_dispatch_offset);
        };
        p_graph.add_task(std::move(task_desc));
    }

    /**
     * @brief Compile a compute shader resource.
     * @param p_shader_id The resource id of the compute shader.
     * @return The compiled compute pipeline.
     * @throws std::runtime_error If the shader resource is not found.
     */
    WBE::Ref<WBE::ROComputePipelineVK> compile_compute_pipeline(std::string_view p_shader_id) {
        auto* renderer = WBE::RendererVK::get_singleton();
        auto shader_resource = WBE::ResourceManager::get_singleton()->get_resource<WBE::RUShader>(p_shader_id);
        if (shader_resource == nullptr) {
            throw std::runtime_error(std::format("Failed to setup pixel sorting test: shader resource {} was not found.", p_shader_id));
        }
        WBE::ComputePipelineCompilerVKContext compute_compiler_context{
            .allocator = get_renderer_allocator().get(),
            .render_data = renderer->get_base_render_data(),
            .render_database = renderer->get_render_database(),
        };
        return WBE::ComputePipelineCompilerVK(compute_compiler_context).compile(p_shader_id, shader_resource).compute_pipeline;
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
