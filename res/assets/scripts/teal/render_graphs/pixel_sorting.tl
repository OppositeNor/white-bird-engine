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
-- Imports: `swap_chain`, which the renderer supplies, `pixel_sort_source` (the source image view), and the
-- application-owned uniform buffers `pixel_sort_contrast` and `pixel_sorting_post_processing_params`.
-- The recipe registers every pipeline it dispatches and draws with.
-- Imported tasks: `draw_imgui`, the application's ImGui overlay, drawn after `present_sorted_pixels`.
-- Exports: `post_processing_output_image`, the result image, which the application reads back for screenshots.

-- Must match the numthreads of write_sorted_pixels.slang and pixel_sorting_post_processing.slang.
local PIXEL_SORT_TILE_SIZE = 16
-- Must match the numthreads of count_pixel_sort_mask.slang and register_pixel_sort_keys.slang, which run one invocation
-- per line.
local PIXEL_SORT_COLUMN_WORKGROUP_SIZE = 64
-- Must match WBE_PIXEL_SORT_COLUMN_OFFSETS_START in pixel_sort_utils.slang.
local PIXEL_SORT_COLUMN_OFFSETS_START = 4
local UINT32_SIZE = 4

local type wbe = require("wbe")
local math_utils = require("utils.math")
local graph = require("render_graph_utils.render_graph_utils")
local sort = require("render_graph_utils.sort")

local ceil_div = math_utils.ceil_div

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

local function make_layout(context: wbe.PixelSortingRecipeContext): Layout
    local width = context.source_width
    local height = context.source_height
    local orientation_name = graph.enum_name(context.orientation)
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

local record Resources
    column_offsets: graph.Resource
    destination: graph.Resource
    post_processing_output_image: graph.Resource
    post_processing_output: graph.Resource
    mask: graph.Resource
    sampler: graph.Resource
    sort_buffers: sort.RadixSortBuffers
    swap_chain: graph.Resource
    source: graph.Resource
    contrast: graph.Resource
    post_processing_params: graph.Resource
end

local function make_resources(image_layout: Layout): Resources
    local extent: wbe.RendExtent2DParamsVK = { width = image_layout.width, height = image_layout.height }
    -- The screenshot reads the result back through a transfer.
    local post_processing_output_image = graph.storage_image({
        name = "pixel_sorting_post_processing_output",
        format = "r16g16b16a16_sfloat",
        extent = extent,
        additional_usage = { "transfer_src" },
    })
    return {
        column_offsets = graph.storage_buffer({
            name = "pixel_sort_column_offsets",
            size = (PIXEL_SORT_COLUMN_OFFSETS_START + image_layout.line_count) * UINT32_SIZE,
        }),
        destination = graph.storage_image({ name = "pixel_sort_destination", format = "r16g16b16a16_sfloat", extent = extent })
            :view(),
        post_processing_output_image = post_processing_output_image,
        post_processing_output = post_processing_output_image:view(),
        -- Stores each masked pixel's rank within its line, with 0xFFFF marking unmasked pixels.
        mask = graph.storage_image({ name = "pixel_sort_mask", format = "r16_uint", extent = extent }):view(),
        sampler = graph.clamp_to_edge_sampler({ name = "pixel_sort_sampler" }),
        -- The register pass writes and the write pass reads the keys; size the sort for every masked pixel.
        sort_buffers = sort.radix_sort_buffers({ name = "pixel_sort", max_element_count = image_layout.element_count }),
        swap_chain = graph.import("swap_chain"),
        source = graph.import("pixel_sort_source"),
        contrast = graph.import("pixel_sort_contrast"),
        post_processing_params = graph.import("pixel_sorting_post_processing_params"),
    }
end

local record KeyRegistrationTasks
    count_mask: graph.Task
    prefix_sum: graph.Task
    register_keys: graph.Task
end

-- Count the masked pixels of every line, sum them into the column offsets, and register their sort keys.
local function add_key_registration(image_layout: Layout, resource_handles: Resources): KeyRegistrationTasks
    local orientation = image_layout.orientation
    local count_mask_pipeline = graph.shader_compute_pipeline("count_pixel_sort_mask_" .. orientation .. "_shader")
    local prefix_sum_pipeline = graph.shader_compute_pipeline("prefix_sum_pixel_sort_columns_shader")
    local register_keys_pipeline = graph.shader_compute_pipeline("register_pixel_sort_keys_" .. orientation .. "_shader")
    local count_mask = graph.compute({
        pipeline = count_mask_pipeline,
        group_count_x = image_layout.column_group_count,
        group_count_y = 1,
    }, { name = "count_pixel_sort_mask", bindings = {
        pixel_sort_source = resource_handles.source,
        pixel_sort_contrast = resource_handles.contrast,
        pixel_sort_column_offsets = resource_handles.column_offsets,
    } })
    -- The prefix sum runs in a single workgroup.
    local prefix_sum = graph.compute({
        pipeline = prefix_sum_pipeline, group_count_x = 1, group_count_y = 1,
    }, { name = "prefix_sum_pixel_sort_columns", bindings = { pixel_sort_column_offsets = resource_handles.column_offsets } })
    local register_keys = graph.compute({
        pipeline = register_keys_pipeline,
        group_count_x = image_layout.column_group_count,
        group_count_y = 1,
    }, { name = "register_pixel_sort_keys", bindings = {
        pixel_sort_source = resource_handles.source,
        pixel_sort_contrast = resource_handles.contrast,
        pixel_sort_column_offsets = resource_handles.column_offsets,
        pixel_sort_keys = resource_handles.sort_buffers.keys,
        pixel_sort_mask = resource_handles.mask,
    } })
    return { count_mask = count_mask, prefix_sum = prefix_sum, register_keys = register_keys }
end

local record OutputTasks
    write_pixels: graph.Task
    post_processing: graph.Task
    present: graph.Task
    draw_imgui: graph.Task
end

-- Write the sorted pixels, post-process them, and present the result under the application's ImGui overlay.
local function add_output(image_layout: Layout, resource_handles: Resources, swap_chain_format: wbe.ImageFormatVK): OutputTasks
    local tile_group_count_x = image_layout.tile_group_count_x
    local tile_group_count_y = image_layout.tile_group_count_y
    local write_pixels_pipeline = graph.shader_compute_pipeline("write_sorted_pixels_" .. image_layout.orientation .. "_shader")
    local post_processing_pipeline = graph.shader_compute_pipeline("pixel_sorting_post_processing_shader")
    local present_pipeline = graph.graphics_pipeline("present_pixel_sort_pipeline", {
        pipeline = "present_pixel_sort_pipeline",
        color_formats = { swap_chain_format },
    })
    local write_pixels = graph.compute({
        pipeline = write_pixels_pipeline,
        group_count_x = tile_group_count_x,
        group_count_y = tile_group_count_y,
    }, { name = "write_sorted_pixels", bindings = {
        pixel_sort_source = resource_handles.source,
        pixel_sort_keys = resource_handles.sort_buffers.keys,
        pixel_sort_mask = resource_handles.mask,
        pixel_sort_contrast = resource_handles.contrast,
        pixel_sort_column_offsets = resource_handles.column_offsets,
        pixel_sort_destination = resource_handles.destination,
    } })
    local post_processing = graph.compute({
        pipeline = post_processing_pipeline,
        group_count_x = tile_group_count_x,
        group_count_y = tile_group_count_y,
    }, { name = "pixel_sorting_post_processing", bindings = {
        pixel_sort_destination = resource_handles.destination,
        pixel_sort_sampler = resource_handles.sampler,
        pixel_sorting_post_processing_params = resource_handles.post_processing_params,
        pixel_sorting_post_processing_output = resource_handles.post_processing_output,
    } })
    local present = graph.draw({
        pipeline = present_pipeline,
        clear_values = { { color = { 0.0, 0.0, 0.0, 1.0 } } },
        draw_vertex_count = 3,
    }, { name = "present_sorted_pixels", bindings = {
        pixel_sorting_post_processing_output = resource_handles.post_processing_output,
        pixel_sort_sampler = resource_handles.sampler,
    } })
    present:color_target(0, resource_handles.swap_chain)
    local draw_imgui = graph.import_task("draw_imgui")
    return { write_pixels = write_pixels, post_processing = post_processing, present = present, draw_imgui = draw_imgui }
end

-- Order the line bookkeeping and the keys before the write, and the output images through post-processing and
-- presentation.
local function connect_tasks(resource_handles: Resources, resource_registration: KeyRegistrationTasks, sort_task: graph.Task,
    output_tasks: OutputTasks)
    graph.chain(resource_handles.column_offsets,
        { resource_registration.count_mask, resource_registration.prefix_sum, resource_registration.register_keys, output_tasks.write_pixels })
    graph.chain(resource_handles.mask, { resource_registration.register_keys, output_tasks.write_pixels })
    graph.chain(resource_handles.sort_buffers.keys, { resource_registration.register_keys, sort_task, output_tasks.write_pixels })
    graph.chain(resource_handles.destination, { output_tasks.write_pixels, output_tasks.post_processing })
    graph.chain(resource_handles.post_processing_output, { output_tasks.post_processing, output_tasks.present })
    graph.chain(resource_handles.swap_chain, { output_tasks.present, output_tasks.draw_imgui })
end

local exports = {}

function exports.build(context: wbe.PixelSortingRecipeContext, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local layout = make_layout(context)
    local resources = make_resources(layout)
    local registration = add_key_registration(layout, resources)
    local radix_sort = sort.radix_sort_task(resources.sort_buffers, { name = "radix_sort" })
    local output = add_output(layout, resources, context.swap_chain_format)
    connect_tasks(resources, registration, radix_sort, output)
    return graph.build({
        tasks = {
            registration.count_mask,
            registration.prefix_sum,
            registration.register_keys,
            radix_sort,
            output.write_pixels,
            output.post_processing,
            output.present,
            output.draw_imgui,
        },
        exports = { post_processing_output_image = resources.post_processing_output_image },
    })
end

return exports
