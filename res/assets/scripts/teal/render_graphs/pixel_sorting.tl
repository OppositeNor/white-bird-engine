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
local UINT32_SIZE = 4

local type wbe = require("wbe")
local math_utils = require("utils.math")
local graph_utils = require("utils.render_graph")
local sort = require("utils.sort")

local ceil_div = math_utils.ceil_div
local storage_buffer = graph_utils.storage_buffer
local storage_image = graph_utils.storage_image
local image_view = graph_utils.image_view
local clamp_to_edge_sampler = graph_utils.clamp_to_edge_sampler

-- The sizes and dispatch counts derived from the source image.
local record Layout
    width: integer
    height: integer
    -- The orientation as the shader resource keys name it, in lower case.
    orientation: string
    -- The number of sorted lines.
    line_count: integer
    -- The number of pixels, the most keys the sort may have to sort.
    element_count: integer
    column_group_count: integer
    tile_group_count_x: integer
    tile_group_count_y: integer
end

-- The registered keys, sorted in place, and the radix sort's scratch buffers.
local RADIX_SORT_BUFFERS: sort.RadixSortBuffers = {
    keys = "pixel_sort_keys",
    keys_swap = "pixel_sort_keys_swap",
    histogram = "pixel_sort_histogram",
    dispatch = "radix_sort_dispatch",
}

local function make_layout(p_context: wbe.PixelSortingRecipeContext): Layout
    local width = p_context.source_width
    local height = p_context.source_height
    local orientation_name = graph_utils.enum_name(p_context.orientation)
    if orientation_name ~= "VERTICAL" and orientation_name ~= "HORIZONTAL" then
        error("pixel_sorting: unknown orientation '" .. orientation_name .. "'.")
    end
    local vertical = orientation_name == "VERTICAL"
    local element_count = width * height
    local line_count = vertical and width or height
    return {
        width = width,
        height = height,
        orientation = string.lower(orientation_name),
        line_count = line_count,
        element_count = element_count,
        column_group_count = ceil_div(line_count, PIXEL_SORT_COLUMN_WORKGROUP_SIZE),
        tile_group_count_x = ceil_div(width, PIXEL_SORT_TILE_SIZE),
        tile_group_count_y = ceil_div(height, PIXEL_SORT_TILE_SIZE),
    }
end

local function make_resources(p_layout: Layout): {wbe.RendRecipeResourceVK}
    local width = p_layout.width
    local height = p_layout.height
    local resources: {wbe.RendRecipeResourceVK} = {
        storage_buffer("pixel_sort_column_offsets", (PIXEL_SORT_COLUMN_OFFSETS_START + p_layout.line_count) * UINT32_SIZE),
        storage_image("pixel_sort_destination_image", "r16g16b16a16_sfloat", width, height),
        image_view("pixel_sort_destination", "pixel_sort_destination_image"),
        -- The screenshot reads the result back through a transfer.
        storage_image("pixel_sorting_post_processing_output_image", "r16g16b16a16_sfloat", width, height, { "transfer_src" }),
        image_view("pixel_sorting_post_processing_output", "pixel_sorting_post_processing_output_image"),
        -- Stores each masked pixel's rank within its line, with 0xFFFF marking unmasked pixels.
        storage_image("pixel_sort_mask_image", "r16_uint", width, height),
        image_view("pixel_sort_mask", "pixel_sort_mask_image"),
        clamp_to_edge_sampler("pixel_sort_sampler"),
    }
    -- The register pass writes and the write pass reads the keys; the sort is sized for every pixel inside the mask.
    for _, resource in ipairs(sort.radix_sort_resources(RADIX_SORT_BUFFERS, p_layout.element_count)) do
        resources[#resources + 1] = resource
    end
    return resources
end

local function compute(p_tasks: graph_utils.TaskList, p_id: string, p_pipeline: string, p_group_count_x: integer,
    p_group_count_y: integer, p_aliases?: {string:string})
    local params: wbe.RTParamsRunComputePipelineVK =
        { pipeline = p_pipeline, group_count_x = p_group_count_x, group_count_y = p_group_count_y }
    p_tasks.add({ id = p_id, type = "run_compute_pipeline", params = params, aliases = p_aliases })
end

-- Count the masked pixels of every line, sum them into the column offsets, and register their sort keys.
local function add_key_registration(p_tasks: graph_utils.TaskList, p_layout: Layout)
    local orientation = p_layout.orientation
    compute(p_tasks, "count_pixel_sort_mask", "count_pixel_sort_mask_" .. orientation .. "_shader", p_layout.column_group_count, 1)
    -- The prefix sum runs in a single workgroup.
    compute(p_tasks, "prefix_sum_pixel_sort_columns", "prefix_sum_pixel_sort_columns_shader", 1, 1)
    compute(p_tasks, "register_pixel_sort_keys", "register_pixel_sort_keys_" .. orientation .. "_shader",
        p_layout.column_group_count, 1)
end

-- Write the sorted pixels, post-process them, and present the result under the application's ImGui overlay.
local function add_output(p_tasks: graph_utils.TaskList, p_layout: Layout)
    local tile_group_count_x = p_layout.tile_group_count_x
    local tile_group_count_y = p_layout.tile_group_count_y
    compute(p_tasks, "write_sorted_pixels", "write_sorted_pixels_" .. p_layout.orientation .. "_shader", tile_group_count_x,
        tile_group_count_y)
    compute(p_tasks, "pixel_sorting_post_processing", "pixel_sorting_post_processing_shader", tile_group_count_x,
        tile_group_count_y)
    local present_params: wbe.RTParamsRunGraphicsPipelineVK = {
        pipeline = "present_pixel_sort_pipeline",
        clear_values = { { color = { 0.0, 0.0, 0.0, 1.0 } } },
        draw_vertex_count = 3,
    }
    p_tasks.add({
        id = "present_sorted_pixels",
        type = "run_graphics_pipeline",
        params = present_params,
        aliases = { ["render_target_colors[0]"] = "swap_chain" },
    })
    p_tasks.add({ id = "draw_imgui", import = true })
end

-- Order the line bookkeeping and the keys before the write, and the output images through post-processing and
-- presentation.
local function connect_tasks(p_tasks: graph_utils.TaskList)
    p_tasks.chain("pixel_sort_column_offsets",
        { "count_pixel_sort_mask", "prefix_sum_pixel_sort_columns", "register_pixel_sort_keys", "write_sorted_pixels" })
    p_tasks.chain("pixel_sort_mask", { "register_pixel_sort_keys", "write_sorted_pixels" })
    p_tasks.chain("pixel_sort_keys", { "register_pixel_sort_keys", "radix_sort", "write_sorted_pixels" })
    p_tasks.chain("pixel_sort_destination", { "write_sorted_pixels", "pixel_sorting_post_processing" })
    p_tasks.chain("pixel_sorting_post_processing_output", { "pixel_sorting_post_processing", "present_sorted_pixels" })
    p_tasks.chain("swap_chain", { "present_sorted_pixels", "draw_imgui" })
end

local exports = {}

function exports.build(context: wbe.PixelSortingRecipeContext): wbe.RendGraphRecipeVK
    local layout = make_layout(context)
    local tasks = graph_utils.task_list()
    add_key_registration(tasks, layout)
    tasks.add(sort.radix_sort_task("radix_sort", RADIX_SORT_BUFFERS))
    add_output(tasks, layout)
    connect_tasks(tasks)
    return {
        imports = { "swap_chain", "pixel_sort_source", "pixel_sort_contrast", "pixel_sorting_post_processing_params" },
        resources = make_resources(layout),
        tasks = tasks.tasks,
    }
end

return exports
