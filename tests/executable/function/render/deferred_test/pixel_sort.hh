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
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "i_post_processing.hh"
#include <array>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class PixelSort
 * @brief Post-processing effect that sorts, by luminance, the pixels of every column of the color image whose luminance
 * is inside a contrast range.
 * @details Copies the color image, counts the masked pixels of every column, turns the counts into the range of every
 * column in a key buffer, registers the masked pixels into those ranges, radix sorts the keys in 4 passes of 8 bits, and
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
     * @param p_extent Extent of the color images the effect is applied to.
     * @param p_color_format Format of the color images the effect is applied to.
     * @throws std::runtime_error If a shader resource is missing, or the extent is empty or too large to pack the sort
     * keys into 32 bits.
     */
    PixelSort(RenderAllocator* p_allocator, RendererVK* p_renderer, VkExtent2D p_extent, VkFormat p_color_format);
    ~PixelSort() override = default;
    PixelSort(const PixelSort&) = delete;
    PixelSort(PixelSort&&) = delete;
    PixelSort& operator=(const PixelSort&) = delete;
    PixelSort& operator=(PixelSort&&) = delete;

    std::string_view get_name() const override {
        return "pixel sort";
    }

    std::string_view add_render_tasks(
        RendGraphDescVK& p_graph, std::string_view p_color_image_view, std::string_view p_previous_task_id) const override;

    static constexpr std::string_view RT_COPY_PIXEL_SORT_SOURCE = "copy_pixel_sort_source";
    static constexpr std::string_view RT_COUNT_PIXEL_SORT_MASK = "count_pixel_sort_mask";
    static constexpr std::string_view RT_PREFIX_SUM_PIXEL_SORT_COLUMNS = "prefix_sum_pixel_sort_columns";
    static constexpr std::string_view RT_REGISTER_PIXEL_SORT_KEYS = "register_pixel_sort_keys";
    static constexpr std::string_view RT_WRITE_SORTED_PIXELS = "write_sorted_pixels";

    /** The smallest luminance of a sorted pixel. */
    static constexpr float MIN_LUMINANCE = 0.25F;
    /** The largest luminance of a sorted pixel. */
    static constexpr float MAX_LUMINANCE = 0.8F;

private:
    // Each pass sorts by one 8-bit digit of the 32-bit keys.
    static constexpr uint32_t RADIX_SORT_PASS_COUNT = 4;

    Ref<ROComputePipelineVK> compile(std::string_view p_shader_id) const;
    void add_compute_task(RendGraphDescVK& p_graph,
        std::string_view p_task_id,
        const Ref<ROComputePipelineVK>& p_pipeline,
        VkExtent3D p_dispatch_group_count,
        std::initializer_list<std::pair<std::string_view, std::string_view>> p_aliases) const;

    RendererVK* renderer = nullptr;
    VkExtent2D extent{};
    VkFormat color_format = VK_FORMAT_UNDEFINED;
    uint32_t sub_histogram_num = 1;
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
