-- Copyright 2025 OppositeNor
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

-- Sorts the masked pixels of every column or row of the source image with the GPU radix sort and presents the result.
-- See ApplicationVulkanPixelSortingTest in tests/executable/function/render/vulkan_pixel_sorting_test.cpp.
--
-- The application supplies a wbe.PixelSortingRecipeContext.
-- Imports: `swap_chain`, `pixel_sort_source` (the source image view), and the application-owned uniform buffers
-- `pixel_sort_contrast` and `pixel_sorting_post_processing_params`.
-- Imported tasks: `draw_imgui`, the application's ImGui overlay, drawn after `present_sorted_pixels`.

-- Must match the numthreads of write_sorted_pixels.slang and pixel_sorting_post_processing.slang.
local PIXEL_SORT_TILE_SIZE = 8
-- Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation
-- per line.
local PIXEL_SORT_COLUMN_WORKGROUP_SIZE = 64
-- Must match WBE_PIXEL_SORT_COLUMN_OFFSETS_START in pixel_sort_utils.slang.
local PIXEL_SORT_COLUMN_OFFSETS_START = 4
-- Must match WBE_RADIX_SORT_BUCKET_COUNT, WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP, WBE_RADIX_SORT_MAX_WORKGROUP_COUNT,
-- WBE_RADIX_SORT_DATA_START, WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM, WBE_RADIX_SORT_LOCAL_SORT_DISPATCH_START, and
-- WBE_RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_START in radix_sort_utils.slang.
local RADIX_SORT_BUCKET_COUNT = 256
local RADIX_SORT_ELEMENTS_PER_WORKGROUP = 256
local RADIX_SORT_MAX_WORKGROUP_COUNT = 16384
local RADIX_SORT_DATA_START = 4
local RADIX_SORT_SCAN_HISTOGRAM_NUM = 16
local RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET = 0
-- One VkDispatchIndirectCommand (3 uint32) after the local sort dispatch.
local RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_OFFSET = 12
local RADIX_SORT_DISPATCH_BUFFER_SIZE = 24
-- Each pass sorts by one 8-bit digit of the 32-bit keys.
local RADIX_SORT_PASS_COUNT = 4
local UINT32_SIZE = 4

local type wbe = require("wbe")
local math_utils = require("utils.math")
local graph_utils = require("utils.render_graph")

local ceil_div = math_utils.ceil_div
local storage_buffer = graph_utils.storage_buffer
local storage_image = graph_utils.storage_image
local image_view = graph_utils.image_view

local exports = {}

function exports.build(context: wbe.PixelSortingRecipeContext): wbe.RendGraphRecipeVK
    local width = context.source_width
    local height = context.source_height
    local orientation_name = graph_utils.enum_name(context.orientation)
    if orientation_name ~= "VERTICAL" and orientation_name ~= "HORIZONTAL" then
        error("pixel_sorting: unknown orientation '" .. orientation_name .. "'.")
    end
    local vertical = orientation_name == "VERTICAL"
    -- The shader resource keys name the orientation in lower case.
    local orientation = string.lower(orientation_name)
    local element_count = width * height
    -- The number of sorted lines.
    local line_count = vertical and width or height
    -- The radix sort has one local sort per RADIX_SORT_ELEMENTS_PER_WORKGROUP keys, up to RADIX_SORT_MAX_WORKGROUP_COUNT,
    -- and sizes its buffers for the case where every pixel is inside the mask.
    local max_local_sort_num =
        math_utils.clamp(ceil_div(element_count, RADIX_SORT_ELEMENTS_PER_WORKGROUP), 1, RADIX_SORT_MAX_WORKGROUP_COUNT)
    local max_overflow_num = ceil_div(max_local_sort_num, RADIX_SORT_SCAN_HISTOGRAM_NUM)
    local key_buffer_size = (RADIX_SORT_DATA_START + element_count) * UINT32_SIZE
    -- The metadata, one sub-histogram per local sort, and one overflow histogram per histogram prefix scan workgroup.
    local histogram_buffer_size =
        (RADIX_SORT_DATA_START + (max_local_sort_num + max_overflow_num) * RADIX_SORT_BUCKET_COUNT) * UINT32_SIZE
    local column_group_count = ceil_div(line_count, PIXEL_SORT_COLUMN_WORKGROUP_SIZE)
    local tile_group_count_x = ceil_div(width, PIXEL_SORT_TILE_SIZE)
    local tile_group_count_y = ceil_div(height, PIXEL_SORT_TILE_SIZE)

    local resources: {wbe.RendRecipeResourceVK} = {
        storage_buffer("pixel_sort_column_offsets", (PIXEL_SORT_COLUMN_OFFSETS_START + line_count) * UINT32_SIZE),
        -- The register pass writes and the write pass reads this buffer; the radix sort passes alternate between it and
        -- the swap buffer.
        storage_buffer("pixel_sort_keys", key_buffer_size),
        storage_buffer("pixel_sort_keys_swap", key_buffer_size),
        storage_buffer("pixel_sort_histogram", histogram_buffer_size),
        -- The indirect dispatches of the radix sort passes, written from the local sort count of the registered keys.
        { id = "radix_sort_dispatch", buffer = { usage = "buffer_indirect", size = RADIX_SORT_DISPATCH_BUFFER_SIZE } },
        storage_image("pixel_sort_destination_image", "r16g16b16a16_sfloat", width, height),
        image_view("pixel_sort_destination", "pixel_sort_destination_image"),
        -- The screenshot reads the result back through a transfer.
        storage_image("pixel_sorting_post_processing_output_image", "r16g16b16a16_sfloat", width, height, { "transfer_src" }),
        image_view("pixel_sorting_post_processing_output", "pixel_sorting_post_processing_output_image"),
        -- Stores each masked pixel's rank within its line, with 0xFFFF marking unmasked pixels.
        storage_image("pixel_sort_mask_image", "r16_uint", width, height),
        image_view("pixel_sort_mask", "pixel_sort_mask_image"),
        {
            id = "pixel_sort_sampler",
            sampler = { address_mode_u = "clamp_to_edge", address_mode_v = "clamp_to_edge", address_mode_w = "clamp_to_edge" },
        },
    }

    local task_list = graph_utils.task_list()
    local add_task = task_list.add
    local chain = task_list.chain
    local function compute(p_id: string, p_pipeline: string, p_group_count_x: integer, p_group_count_y: integer,
        p_aliases?: {string:string})
        local params: wbe.RTParamsRunComputePipelineVK =
            { pipeline = p_pipeline, group_count_x = p_group_count_x, group_count_y = p_group_count_y }
        add_task({ id = p_id, type = "run_compute_pipeline", params = params, aliases = p_aliases })
    end
    local function indirect_compute(p_id: string, p_pipeline: string, p_offset: integer, p_aliases: {string:string})
        local params: wbe.RTParamsRunComputePipelineIndirectVK =
            { pipeline = p_pipeline, indirect_buffer = "radix_sort_dispatch", indirect_offset = p_offset }
        add_task({ id = p_id, type = "run_compute_pipeline_indirect", params = params, aliases = p_aliases })
    end

    compute("count_pixel_sort_mask", "count_pixel_sort_mask_" .. orientation .. "_shader", column_group_count, 1)
    -- The prefix sum runs in a single workgroup.
    compute("prefix_sum_pixel_sort_columns", "prefix_sum_pixel_sort_columns_shader", 1, 1)
    compute("register_pixel_sort_keys", "register_pixel_sort_keys_" .. orientation .. "_shader", column_group_count, 1)

    -- The element count is fixed once the keys are registered, so every pass uses the same dispatches.
    compute("radix_sort_prepare_dispatch", "radix_sort_prepare_dispatch", 1, 1,
        { input_buffer = "pixel_sort_keys", dispatch_buffer = "radix_sort_dispatch" })
    local histogram_ids: {string} = {}
    local prefix_sum_ids: {string} = {}
    local overflow_prefix_sum_ids: {string} = {}
    local add_overflow_ids: {string} = {}
    local scatter_ids: {string} = {}
    for pass = 0, RADIX_SORT_PASS_COUNT - 1 do
        local input_keys = pass % 2 == 0 and "pixel_sort_keys" or "pixel_sort_keys_swap"
        local output_keys = pass % 2 == 0 and "pixel_sort_keys_swap" or "pixel_sort_keys"
        local bit_region = string.format("%d_%d", pass * 8, (pass + 1) * 8)
        local index = pass + 1
        histogram_ids[index] = "radix_sort_histogram_" .. bit_region
        prefix_sum_ids[index] = "radix_sort_histogram_prefix_sum_" .. bit_region
        overflow_prefix_sum_ids[index] = "radix_sort_histogram_overflow_prefix_sum_" .. bit_region
        add_overflow_ids[index] = "radix_sort_histogram_add_overflow_" .. bit_region
        scatter_ids[index] = "radix_sort_scatter_" .. bit_region
        indirect_compute(histogram_ids[index], "radix_sort_histogram_workgroup_process_" .. bit_region,
            RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET, { input_buffer = input_keys, histogram_buffer = "pixel_sort_histogram" })
        indirect_compute(prefix_sum_ids[index], "radix_sort_histogram_prefix_sum", RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_OFFSET,
            { histogram_buffer = "pixel_sort_histogram" })
        -- The overflow histograms are scanned in a single workgroup.
        compute(overflow_prefix_sum_ids[index], "radix_sort_histogram_overflow_prefix_sum", 1, 1,
            { histogram_buffer = "pixel_sort_histogram" })
        indirect_compute(add_overflow_ids[index], "radix_sort_histogram_add_overflow", RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
            { histogram_buffer = "pixel_sort_histogram" })
        indirect_compute(scatter_ids[index], "radix_sort_scatter_" .. bit_region, RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
            { input_buffer = input_keys, histogram_buffer = "pixel_sort_histogram", output_buffer = output_keys })
    end

    -- After an even number of passes, the sorted keys are back in the key buffer.
    compute("write_sorted_pixels", "write_sorted_pixels_" .. orientation .. "_shader", tile_group_count_x, tile_group_count_y)
    compute("pixel_sorting_post_processing", "pixel_sorting_post_processing_shader", tile_group_count_x, tile_group_count_y)
    local present_params: wbe.RTParamsRunGraphicsPipelineVK = {
        pipeline = "present_pixel_sort_pipeline",
        clear_values = { { color = { 0.0, 0.0, 0.0, 1.0 } } },
        draw_vertex_count = 3,
    }
    add_task({
        id = "present_sorted_pixels",
        type = "run_graphics_pipeline",
        params = present_params,
        aliases = { ["render_target_colors[0]"] = "swap_chain" },
    })

    chain("pixel_sort_column_offsets",
        { "count_pixel_sort_mask", "prefix_sum_pixel_sort_columns", "register_pixel_sort_keys", "write_sorted_pixels" })
    chain("pixel_sort_mask", { "register_pixel_sort_keys", "write_sorted_pixels" })
    -- Every task using a key buffer, in execution order. Pass p reads the keys from one buffer and scatters them into the
    -- other, so each buffer is written by every other scatter.
    chain("pixel_sort_keys", {
        "register_pixel_sort_keys",
        "radix_sort_prepare_dispatch",
        histogram_ids[1],
        scatter_ids[1],
        scatter_ids[2],
        histogram_ids[3],
        scatter_ids[3],
        scatter_ids[4],
        "write_sorted_pixels",
    })
    chain("pixel_sort_keys_swap",
        { scatter_ids[1], histogram_ids[2], scatter_ids[2], scatter_ids[3], histogram_ids[4], scatter_ids[4] })
    local histogram_chain: {string} = {}
    -- The prepare task followed by every task dispatched indirectly, in execution order.
    local dispatch_chain: {string} = { "radix_sort_prepare_dispatch" }
    for index = 1, RADIX_SORT_PASS_COUNT do
        for _, id in ipairs({ histogram_ids[index], prefix_sum_ids[index], overflow_prefix_sum_ids[index],
            add_overflow_ids[index], scatter_ids[index] }) do
            histogram_chain[#histogram_chain + 1] = id
        end
        for _, id in ipairs({ histogram_ids[index], prefix_sum_ids[index], add_overflow_ids[index], scatter_ids[index] }) do
            dispatch_chain[#dispatch_chain + 1] = id
        end
    end
    chain("pixel_sort_histogram", histogram_chain)
    chain("radix_sort_dispatch", dispatch_chain)
    chain("pixel_sort_destination", { "write_sorted_pixels", "pixel_sorting_post_processing" })
    chain("pixel_sorting_post_processing_output", { "pixel_sorting_post_processing", "present_sorted_pixels" })
    add_task({ id = "draw_imgui", import = true })
    chain("swap_chain", { "present_sorted_pixels", "draw_imgui" })

    return {
        imports = { "swap_chain", "pixel_sort_source", "pixel_sort_contrast", "pixel_sorting_post_processing_params" },
        resources = resources,
        tasks = task_list.tasks,
    }
end

return exports
