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
local graph = require("render_graph_utils.render_graph_utils")
local sort = require("render_graph_utils.sort")

local ceil_div = math_utils.ceil_div

-- Must match the numthreads of copy_pixel_sort_source.slang and write_sorted_pixels.slang.
local PIXEL_SORT_TILE_SIZE = 16
-- Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation
-- per sorted line.
local PIXEL_SORT_COLUMN_WORKGROUP_SIZE = 64
-- Must match WBE_PIXEL_SORT_COLUMN_OFFSETS_START in pixel_sort_utils.slang.
local PIXEL_SORT_COLUMN_OFFSETS_START = 4
local UINT32_SIZE = 4
local UINT16_MAX = 65535
local UINT32_MAX = 4294967295

local record pixel_sort
    record Target
        -- The color image view sorted in place, bound to the subgraph's `color` port; it must be sampled
        -- and storage.
        color_view: graph.Resource
        -- The width of the color image.
        width: integer
        -- The height of the color image.
        height: integer
    end
end

-- The effect as a subgraph task named name, which sorts the target's color view in place through its `color` port. The
-- params' toggle gates all of its tasks.
function pixel_sort.subgraph(name: string, params: wbe.PixelSortRecipeParams, target: pixel_sort.Target): graph.Task
    local direction = graph.enum_name(params.direction)
    local vertical = direction == "VERTICAL"
    if not vertical and direction ~= "HORIZONTAL" then
        error("pixel_sort: unknown direction '" .. direction .. "'.")
    end
    local scope = graph.enum_name(params.scope)
    if scope ~= "LINE" and scope ~= "SPAN" then
        error("pixel_sort: unknown scope '" .. scope .. "'.")
    end
    local width = target.width
    local height = target.height
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

    local color = graph.port("color")
    local contrast = graph.import(params.contrast_import)
    local extent: wbe.RendExtent2DParamsVK = { width = width, height = height }
    local source = graph.storage_image({ name = "pixel_sort_source", format = params.color_format, extent = extent })
        :view()
    local mask = graph.storage_image({
        name = "pixel_sort_mask",
        format = scope == "SPAN" and "r16g16b16a16_uint" or "r16_uint",
        extent = extent,
    }):view()
    local column_offsets = graph.storage_buffer({
        name = "pixel_sort_column_offsets",
        size = (PIXEL_SORT_COLUMN_OFFSETS_START + line_count) * UINT32_SIZE,
    })
    -- The sort is sized for every pixel inside the mask.
    local sort_buffers = sort.radix_sort_buffers({ name = "pixel_sort", max_element_count = element_count })

    local direction_suffix = "_" .. string.lower(direction) .. "_shader"
    local copy_source_pipeline = graph.shader_compute_pipeline("copy_pixel_sort_source_shader")
    local count_mask_pipeline = graph.shader_compute_pipeline("count_pixel_sort_mask" .. direction_suffix)
    local prefix_sum_pipeline = graph.shader_compute_pipeline("prefix_sum_pixel_sort_columns_shader")
    local register_keys_pipeline = graph.shader_compute_pipeline("register_pixel_sort_keys" .. direction_suffix)
    local write_pixels_pipeline = graph.shader_compute_pipeline("write_sorted_pixels" .. direction_suffix)

    local copy_source = graph.compute({
        pipeline = copy_source_pipeline,
        group_count_x = tile_group_count_x,
        group_count_y = tile_group_count_y,
        queue_family = params.queue_family,
    }, { name = "copy_pixel_sort_source", bindings = { pixel_sort_input = color, pixel_sort_source = source } })
    local count_mask = graph.compute({
        pipeline = count_mask_pipeline,
        group_count_x = column_group_count,
        group_count_y = 1,
        queue_family = params.queue_family,
    }, { name = "count_pixel_sort_mask", bindings = {
        pixel_sort_source = source,
        pixel_sort_contrast = contrast,
        pixel_sort_column_offsets = column_offsets,
    } })
    -- The prefix sums run in a single workgroup.
    local prefix_sum = graph.compute({
        pipeline = prefix_sum_pipeline,
        group_count_x = 1,
        group_count_y = 1,
        queue_family = params.queue_family,
    }, { name = "prefix_sum_pixel_sort_columns", bindings = { pixel_sort_column_offsets = column_offsets } })
    local register_keys = graph.compute({
        pipeline = register_keys_pipeline,
        group_count_x = column_group_count,
        group_count_y = 1,
        queue_family = params.queue_family,
    }, { name = "register_pixel_sort_keys", bindings = {
        pixel_sort_source = source,
        pixel_sort_contrast = contrast,
        pixel_sort_column_offsets = column_offsets,
        pixel_sort_keys = sort_buffers.keys,
        pixel_sort_mask = mask,
    } })

    local radix_sort = sort.radix_sort_task(sort_buffers, { name = "pixel_sort_radix_sort", queue_family = params.queue_family })
    local write_pixels = graph.compute({
        pipeline = write_pixels_pipeline,
        group_count_x = tile_group_count_x,
        group_count_y = tile_group_count_y,
        queue_family = params.queue_family,
    }, { name = "write_sorted_pixels", bindings = {
        pixel_sort_source = source,
        pixel_sort_keys = sort_buffers.keys,
        pixel_sort_mask = mask,
        pixel_sort_contrast = contrast,
        pixel_sort_column_offsets = column_offsets,
        pixel_sort_destination = color,
    } })

    graph.chain(color, { copy_source, write_pixels })
    graph.chain(source, { copy_source, count_mask, register_keys, write_pixels })
    graph.chain(column_offsets, { count_mask, prefix_sum, register_keys, write_pixels })
    graph.chain(mask, { register_keys, write_pixels })
    graph.chain(sort_buffers.keys, { register_keys, radix_sort, write_pixels })
    return graph.subgraph({
        name = name,
        tasks = { copy_source, count_mask, prefix_sum, register_keys, radix_sort, write_pixels },
        ports = { color = target.color_view },
        toggle = { event = params.toggle_event, enabled = params.enabled },
    })
end

return pixel_sort
