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

local type wbe = require("wbe")
local math_utils = require("utils.math")
local graph_utils = require("utils.render_graph")
local sort = require("utils.sort")

local ceil_div = math_utils.ceil_div

-- Must match the numthreads of copy_pixel_sort_source.slang and write_sorted_pixels.slang.
local PIXEL_SORT_TILE_SIZE = 8
-- Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation
-- per sorted line.
local PIXEL_SORT_COLUMN_WORKGROUP_SIZE = 64
-- Must match WBE_PIXEL_SORT_COLUMN_OFFSETS_START in pixel_sort_utils.slang.
local PIXEL_SORT_COLUMN_OFFSETS_START = 4
local UINT32_SIZE = 4
local UINT16_MAX = 65535
local UINT32_MAX = 4294967295

-- The registered keys, sorted in place, and the radix sort's scratch buffers.
local RADIX_SORT_BUFFERS: sort.RadixSortBuffers = {
    keys = "pixel_sort_keys",
    keys_swap = "pixel_sort_keys_swap",
    histogram = "pixel_sort_histogram",
    dispatch = "pixel_sort_radix_sort_dispatch",
}

local record pixel_sort
    record Target
        -- The graph id of the color image view sorted in place, bound to the subgraph's `color` port; it must be sampled
        -- and storage.
        color_view: string
        -- The width of the color image.
        width: integer
        -- The height of the color image.
        height: integer
    end
end

-- The effect as a subgraph task with id p_id, which sorts the target's color view in place through its `color` port. The
-- params' toggle gates all of its tasks.
function pixel_sort.subgraph(p_id: string, p_params: wbe.PixelSortRecipeParams, p_target: pixel_sort.Target): wbe.RendRecipeTaskVK
    local direction = graph_utils.enum_name(p_params.direction)
    local vertical = direction == "VERTICAL"
    if not vertical and direction ~= "HORIZONTAL" then
        error("pixel_sort: unknown direction '" .. direction .. "'.")
    end
    local scope = graph_utils.enum_name(p_params.scope)
    if scope ~= "LINE" and scope ~= "SPAN" then
        error("pixel_sort: unknown scope '" .. scope .. "'.")
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
    local tile_group_count_x = ceil_div(width, PIXEL_SORT_TILE_SIZE)
    local tile_group_count_y = ceil_div(height, PIXEL_SORT_TILE_SIZE)
    local column_group_count = ceil_div(line_count, PIXEL_SORT_COLUMN_WORKGROUP_SIZE)

    local resources: {wbe.RendRecipeResourceVK} = {}
    local function add_resource(p_resource: wbe.RendRecipeResourceVK)
        resources[#resources + 1] = p_resource
    end
    add_resource(graph_utils.storage_image("pixel_sort_source_image", p_params.color_format, width, height))
    add_resource(graph_utils.image_view("pixel_sort_source", "pixel_sort_source_image"))
    add_resource(graph_utils.storage_image(
        "pixel_sort_mask_image", scope == "SPAN" and "r16g16b16a16_uint" or "r16_uint", width, height))
    add_resource(graph_utils.image_view("pixel_sort_mask", "pixel_sort_mask_image"))
    add_resource(graph_utils.storage_buffer("pixel_sort_column_offsets", (PIXEL_SORT_COLUMN_OFFSETS_START + line_count) * UINT32_SIZE))
    -- The sort is sized for every pixel inside the mask.
    for _, resource in ipairs(sort.radix_sort_resources(RADIX_SORT_BUFFERS, element_count)) do
        add_resource(resource)
    end

    local tasks = graph_utils.task_list()
    local function compute(p_task_id: string, p_pipeline: string, p_group_count_x: integer, p_group_count_y: integer,
        p_aliases?: {string:string})
        local params: wbe.RTParamsRunComputePipelineVK = {
            pipeline = p_pipeline,
            group_count_x = p_group_count_x,
            group_count_y = p_group_count_y,
            queue_family = p_params.queue_family,
        }
        tasks.add({ id = p_task_id, type = "run_compute_pipeline", params = params, aliases = p_aliases })
    end
    local direction_suffix = "_" .. string.lower(direction) .. "_shader"

    compute("copy_pixel_sort_source", "copy_pixel_sort_source_shader", tile_group_count_x, tile_group_count_y,
        { pixel_sort_input = "color" })
    compute("count_pixel_sort_mask", "count_pixel_sort_mask" .. direction_suffix, column_group_count, 1)
    -- The prefix sums run in a single workgroup.
    compute("prefix_sum_pixel_sort_columns", "prefix_sum_pixel_sort_columns_shader", 1, 1)
    compute("register_pixel_sort_keys", "register_pixel_sort_keys" .. direction_suffix, column_group_count, 1)

    tasks.add(sort.radix_sort_task("pixel_sort_radix_sort", RADIX_SORT_BUFFERS, p_params.queue_family))
    compute("write_sorted_pixels", "write_sorted_pixels" .. direction_suffix, tile_group_count_x, tile_group_count_y,
        { pixel_sort_destination = "color" })

    local chain = tasks.chain
    chain("color", { "copy_pixel_sort_source", "write_sorted_pixels" })
    chain("pixel_sort_source", { "copy_pixel_sort_source", "count_pixel_sort_mask", "register_pixel_sort_keys", "write_sorted_pixels" })
    chain("pixel_sort_column_offsets",
        { "count_pixel_sort_mask", "prefix_sum_pixel_sort_columns", "register_pixel_sort_keys", "write_sorted_pixels" })
    chain("pixel_sort_mask", { "register_pixel_sort_keys", "write_sorted_pixels" })
    chain("pixel_sort_keys", { "register_pixel_sort_keys", "pixel_sort_radix_sort", "write_sorted_pixels" })
    local subgraph = graph_utils.subgraph(p_id,
        { imports = { p_params.contrast_import }, resources = resources, tasks = tasks.tasks, ports = { "color" } },
        { color = p_target.color_view })
    subgraph.toggle = { event = p_params.toggle_event, enabled = p_params.enabled }
    return subgraph
end

return pixel_sort
