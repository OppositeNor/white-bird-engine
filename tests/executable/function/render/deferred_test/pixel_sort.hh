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
#ifndef WBE_FILE_PIXEL_SORT_HH
#define WBE_FILE_PIXEL_SORT_HH

#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "i_post_processing.hh"
#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @enum PixelSortDirection
 * @brief The lines of the image that the pixel sort sorts.
 */
enum class PixelSortDirection : uint8_t {
    /** Sort every column, from the top. */
    VERTICAL,
    /** Sort every row, from the left. */
    HORIZONTAL,
};

/**
 * @enum PixelSortMetric
 * @brief The value the pixel sort orders the pixels of every line by.
 * @details The values must match the WBE_PIXEL_SORT_METRIC_ macros in pixel_sort_utils.slang.
 */
enum class PixelSortMetric : uint32_t {
    /** Sort by luminance, darkest first. */
    LUMINANCE = 0,
    /** Sort by hue, from red through green and blue to magenta. */
    HUE = 1,
    /** Sort by saturation, grayest first. */
    SATURATION = 2,
};

/**
 * @enum PixelSortScope
 * @brief The pixels each sort of the pixel sort covers.
 * @details The values must match the WBE_PIXEL_SORT_SCOPE_ macros in pixel_sort_utils.slang.
 */
enum class PixelSortScope : uint32_t {
    /** Sort all masked pixels of every line together, compacting them across unmasked gaps. */
    LINE = 0,
    /** Sort every span, a maximal run of consecutive masked pixels of a line, independently. */
    SPAN = 1,
};

/**
 * @class PixelSort
 * @brief Post-processing effect that sorts, by a selectable metric, the pixels of every column or row of the color image
 * whose sort metric is inside a contrast range, either across the whole line or within every span of consecutive masked
 * pixels.
 * @details Copies the color image, counts the masked pixels of every sorted line, turns the counts into the range of
 * every line in a key buffer, registers the masked pixels into those ranges, radix sorts the keys in 4 passes of 8 bits, and
 * writes every masked pixel back from the sorted key at its registered index (see the pixel_sort and sort shaders). The
 * pass resources are graph-owned, so every graph the effect is added to allocates its own. Neither copyable nor movable.
 */
class PixelSort final : public IPostProcessing {
public:
    PixelSort() = delete;
    /**
     * @brief Compile the effect's compute pipelines.
     * @param p_allocator Allocator of the effect's pipelines and tasks; must outlive the effect.
     * @param p_renderer The renderer; must outlive the effect.
     * @param p_extent Factory of the extent of the color images the effect is applied to. Every graph the effect is added to
     * evaluates it against its render graph context, and resizes the effect's resources and dispatches with it.
     * @param p_color_format Format of the color images the effect is applied to.
     * @param p_direction Whether the effect sorts the columns or the rows of the image.
     * @param p_metric The value the effect orders the pixels of every line by, and masks them by.
     * @param p_scope Whether each sort covers a whole line or one span of consecutive masked pixels.
     * @throws std::runtime_error If a shader resource is missing, p_extent is null, the extent of the current renderer
     * state is empty or too large to pack the sort keys into 32 bits, the length of a sorted line does not fit the 16-bit
     * mask image, p_metric is not a PixelSortMetric value, or p_scope is not a PixelSortScope value. Graphs throw the same
     * extent errors when a later context yields an invalid extent.
     */
    PixelSort(RenderAllocator* p_allocator,
        RendererVK* p_renderer,
        std::function<VkExtent2D(const RendGraphContextVK&)> p_extent,
        VkFormat p_color_format,
        PixelSortDirection p_direction,
        PixelSortMetric p_metric,
        PixelSortScope p_scope);
    virtual ~PixelSort() override = default;
    PixelSort(const PixelSort&) = delete;
    PixelSort(PixelSort&&) = delete;
    PixelSort& operator=(const PixelSort&) = delete;
    PixelSort& operator=(PixelSort&&) = delete;

    virtual std::string_view get_name() const override {
        return "pixel sort";
    }

    virtual std::string_view add_render_tasks(
        RendGraphDescVK& p_graph, std::string_view p_color_image_view, std::string_view p_previous_task_id) const override;

    static constexpr std::string_view RT_COPY_PIXEL_SORT_SOURCE = "copy_pixel_sort_source";
    static constexpr std::string_view RT_COUNT_PIXEL_SORT_MASK = "count_pixel_sort_mask";
    static constexpr std::string_view RT_PREFIX_SUM_PIXEL_SORT_COLUMNS = "prefix_sum_pixel_sort_columns";
    static constexpr std::string_view RT_REGISTER_PIXEL_SORT_KEYS = "register_pixel_sort_keys";
    static constexpr std::string_view RT_WRITE_SORTED_PIXELS = "write_sorted_pixels";

    /** The smallest sort metric of a sorted pixel. */
    static constexpr float MIN_METRIC = 0.3F;
    /** The largest sort metric of a sorted pixel. */
    static constexpr float MAX_METRIC = 1.1F;

private:
    // Each pass sorts by one 8-bit digit of the 32-bit keys.
    static constexpr uint32_t RADIX_SORT_PASS_COUNT = 4;

    // The sizes of the resources and dispatches of the effect for one extent of the color image.
    struct Layout;

    static Layout make_layout(VkExtent2D p_extent, PixelSortDirection p_direction);
    Ref<ROComputePipelineVK> compile(std::string_view p_shader_id) const;
    // Add a task dispatching p_pipeline with the workgroup counts of the layout of the graph's context.
    void add_compute_task(RendGraphDescVK& p_graph,
        std::string_view p_task_id,
        const Ref<ROComputePipelineVK>& p_pipeline,
        VkExtent3D Layout::* p_group_count,
        std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) const;

    RendererVK* renderer = nullptr;
    std::function<VkExtent2D(const RendGraphContextVK&)> extent;
    VkFormat color_format = VK_FORMAT_UNDEFINED;
    PixelSortDirection direction = PixelSortDirection::VERTICAL;
    PixelSortMetric metric = PixelSortMetric::SATURATION;
    PixelSortScope scope = PixelSortScope::LINE;
    Ref<ROComputePipelineVK> copy_source_pipeline;
    Ref<ROComputePipelineVK> count_mask_pipeline;
    Ref<ROComputePipelineVK> prefix_sum_columns_pipeline;
    Ref<ROComputePipelineVK> register_keys_pipeline;
    Ref<ROComputePipelineVK> write_sorted_pixels_pipeline;
    Ref<ROComputePipelineVK> histogram_prefix_sum_pipeline;
    std::array<Ref<ROComputePipelineVK>, RADIX_SORT_PASS_COUNT> histogram_pipelines;
    std::array<Ref<ROComputePipelineVK>, RADIX_SORT_PASS_COUNT> scatter_pipelines;
};

} // namespace WhiteBirdEngine

#endif
