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

-- In-place pixel sort post-processing of a color image, for recipes that include it as a feature. Sorts the masked pixels
-- of every column or row with the GPU radix sort. The application compiles the pipelines and owns the contrast buffer;
-- see PixelSort in tests/executable/function/render/deferred_test/pixel_sort.hh.

local math_utils = require("utils.math")
local graph_utils = require("utils.render_graph")

local ceil_div = math_utils.ceil_div

-- Must match the numthreads of copy_pixel_sort_source.slang and write_sorted_pixels.slang.
local PIXEL_SORT_TILE_SIZE = 8
-- Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation
-- per sorted line.
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
local UINT16_MAX = 65535
local UINT32_MAX = 4294967295

local DISPATCH_BUFFER = "pixel_sort_radix_sort_dispatch"

local exports = {}

---@class wbe.features.PixelSortParams
---@field direction "vertical"|"horizontal" The lines sorted.
---@field scope "line"|"span" Whether each sort covers the whole line or each masked span.
---@field color_format wbe.ImageFormatVK The format of the color image.
---@field queue_family wbe.QueueFamilyVK The queue every task runs on.
---@field contrast_import string The import name of the application's contrast uniform buffer.
---@field toggle_event string The event id every task's toggle receives RTToggleVK commands under.
---@field enabled boolean Whether the tasks start enabled.

---@class wbe.features.PixelSortTarget
---@field color_view string The graph id of the color image view sorted in place; it must be sampled and storage.
---@field previous_task string The last task that accesses color_view before the effect.
---@field width integer The width of the color image.
---@field height integer The height of the color image.

---Add the effect's imports, resources, and tasks to a recipe under construction.
---@param p_graph { imports: string[], resources: wbe.RendRecipeResourceVK[], tasks: wbe.utils.TaskList }
---@param p_params wbe.features.PixelSortParams
---@param p_target wbe.features.PixelSortTarget
---@return string last_task The effect's last task that accesses the color view; connect later users of the image to it.
function exports.add(p_graph, p_params, p_target)
    local vertical = p_params.direction == "vertical"
    if not vertical and p_params.direction ~= "horizontal" then
        error("pixel_sort: unknown direction '" .. tostring(p_params.direction) .. "'.")
    end
    if p_params.scope ~= "line" and p_params.scope ~= "span" then
        error("pixel_sort: unknown scope '" .. tostring(p_params.scope) .. "'.")
    end
    local width = p_target.width
    local height = p_target.height
    local line_length = vertical and height or width
    local element_count = width * height
    -- The mask image stores each masked pixel's rank within its line, and with the span scope its span offset and span
    -- length, as 16-bit channels, with 0xFFFF marking unmasked pixels.
    if line_length >= UINT16_MAX then
        error(string.format("pixel_sort: a %d-pixel sorted line is too long for the 16-bit pixel sort mask.", line_length))
    end
    if element_count == 0 then
        error(string.format("pixel_sort: a %dx%d image is empty.", width, height))
    end
    -- A key packs the line, metric, and position along the line, so at least 2 metric levels must fit in 32 bits.
    if UINT32_MAX // element_count < 2 then
        error(string.format("pixel_sort: a %dx%d image is too large to pack its sort keys into 32 bits.", width, height))
    end

    local line_count = vertical and width or height
    -- The radix sort has one local sort per RADIX_SORT_ELEMENTS_PER_WORKGROUP keys, up to RADIX_SORT_MAX_WORKGROUP_COUNT,
    -- and sizes its buffers for the case where every pixel is inside the mask.
    local max_local_sort_num =
        math_utils.clamp(ceil_div(element_count, RADIX_SORT_ELEMENTS_PER_WORKGROUP), 1, RADIX_SORT_MAX_WORKGROUP_COUNT)
    local max_overflow_num = ceil_div(max_local_sort_num, RADIX_SORT_SCAN_HISTOGRAM_NUM)
    local key_buffer_size = (RADIX_SORT_DATA_START + element_count) * UINT32_SIZE
    local tile_group_count_x = ceil_div(width, PIXEL_SORT_TILE_SIZE)
    local tile_group_count_y = ceil_div(height, PIXEL_SORT_TILE_SIZE)
    local column_group_count = ceil_div(line_count, PIXEL_SORT_COLUMN_WORKGROUP_SIZE)

    p_graph.imports[#p_graph.imports + 1] = p_params.contrast_import
    local resources = p_graph.resources
    local function add_resource(p_resource)
        resources[#resources + 1] = p_resource
    end
    add_resource(graph_utils.storage_image("pixel_sort_source_image", p_params.color_format, width, height))
    add_resource(graph_utils.image_view("pixel_sort_source", "pixel_sort_source_image"))
    add_resource(graph_utils.storage_image(
        "pixel_sort_mask_image", p_params.scope == "span" and "r16g16b16a16_uint" or "r16_uint", width, height))
    add_resource(graph_utils.image_view("pixel_sort_mask", "pixel_sort_mask_image"))
    add_resource(graph_utils.storage_buffer("pixel_sort_column_offsets", (PIXEL_SORT_COLUMN_OFFSETS_START + line_count) * UINT32_SIZE))
    add_resource(graph_utils.storage_buffer("pixel_sort_keys", key_buffer_size))
    add_resource(graph_utils.storage_buffer("pixel_sort_keys_swap", key_buffer_size))
    -- The metadata, one sub-histogram per local sort, and one overflow histogram per histogram prefix scan workgroup.
    add_resource(graph_utils.storage_buffer("pixel_sort_histogram",
        (RADIX_SORT_DATA_START + (max_local_sort_num + max_overflow_num) * RADIX_SORT_BUCKET_COUNT) * UINT32_SIZE))
    -- The indirect dispatches of the radix sort passes, written from the local sort count of the registered keys.
    add_resource({ id = DISPATCH_BUFFER, buffer = { usage = "buffer_indirect", size = RADIX_SORT_DISPATCH_BUFFER_SIZE } })

    local tasks = p_graph.tasks
    local toggle = { event = p_params.toggle_event, enabled = p_params.enabled }
    local function compute(p_id, p_pipeline, p_group_count_x, p_group_count_y, p_aliases)
        tasks.add({
            id = p_id,
            type = "run_compute_pipeline",
            params = {
                pipeline = p_pipeline,
                group_count_x = p_group_count_x,
                group_count_y = p_group_count_y,
                queue_family = p_params.queue_family,
            },
            aliases = p_aliases,
            toggle = toggle,
        })
    end
    local function indirect_compute(p_id, p_pipeline, p_offset, p_aliases)
        tasks.add({
            id = p_id,
            type = "run_compute_pipeline_indirect",
            params = {
                pipeline = p_pipeline,
                indirect_buffer = DISPATCH_BUFFER,
                indirect_offset = p_offset,
                queue_family = p_params.queue_family,
            },
            aliases = p_aliases,
            toggle = toggle,
        })
    end
    local direction_suffix = "_" .. p_params.direction .. "_shader"

    compute("copy_pixel_sort_source", "copy_pixel_sort_source_shader", tile_group_count_x, tile_group_count_y,
        { pixel_sort_input = p_target.color_view })
    compute("count_pixel_sort_mask", "count_pixel_sort_mask" .. direction_suffix, column_group_count, 1)
    -- The prefix sums run in a single workgroup.
    compute("prefix_sum_pixel_sort_columns", "prefix_sum_pixel_sort_columns_shader", 1, 1)
    compute("register_pixel_sort_keys", "register_pixel_sort_keys" .. direction_suffix, column_group_count, 1)

    -- The element count is fixed once the keys are registered, so every pass uses the same dispatches.
    compute("pixel_sort_radix_sort_prepare_dispatch", "radix_sort_prepare_dispatch", 1, 1,
        { input_buffer = "pixel_sort_keys", dispatch_buffer = DISPATCH_BUFFER })
    local histogram_ids, prefix_sum_ids, overflow_prefix_sum_ids, add_overflow_ids, scatter_ids = {}, {}, {}, {}, {}
    for pass = 0, RADIX_SORT_PASS_COUNT - 1 do
        local input_keys = pass % 2 == 0 and "pixel_sort_keys" or "pixel_sort_keys_swap"
        local output_keys = pass % 2 == 0 and "pixel_sort_keys_swap" or "pixel_sort_keys"
        local bit_region = string.format("%d_%d", pass * 8, (pass + 1) * 8)
        local index = pass + 1
        histogram_ids[index] = "pixel_sort_radix_sort_histogram_" .. bit_region
        prefix_sum_ids[index] = "pixel_sort_radix_sort_histogram_prefix_sum_" .. bit_region
        overflow_prefix_sum_ids[index] = "pixel_sort_radix_sort_histogram_overflow_prefix_sum_" .. bit_region
        add_overflow_ids[index] = "pixel_sort_radix_sort_histogram_add_overflow_" .. bit_region
        scatter_ids[index] = "pixel_sort_radix_sort_scatter_" .. bit_region
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
    compute("write_sorted_pixels", "write_sorted_pixels" .. direction_suffix, tile_group_count_x, tile_group_count_y,
        { pixel_sort_destination = p_target.color_view })

    local chain = tasks.chain
    chain(p_target.color_view, { p_target.previous_task, "copy_pixel_sort_source", "write_sorted_pixels" })
    chain("pixel_sort_source", { "copy_pixel_sort_source", "count_pixel_sort_mask", "register_pixel_sort_keys", "write_sorted_pixels" })
    chain("pixel_sort_column_offsets",
        { "count_pixel_sort_mask", "prefix_sum_pixel_sort_columns", "register_pixel_sort_keys", "write_sorted_pixels" })
    chain("pixel_sort_mask", { "register_pixel_sort_keys", "write_sorted_pixels" })
    -- Every task using a key buffer, in execution order. Pass p reads the keys from one buffer and scatters them into the
    -- other, so each buffer is written by every other scatter.
    chain("pixel_sort_keys", {
        "register_pixel_sort_keys",
        "pixel_sort_radix_sort_prepare_dispatch",
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
    local histogram_chain = {}
    -- The prepare task followed by every task dispatched indirectly, in execution order.
    local dispatch_chain = { "pixel_sort_radix_sort_prepare_dispatch" }
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
    chain(DISPATCH_BUFFER, dispatch_chain)
    return "write_sorted_pixels"
end

return exports
