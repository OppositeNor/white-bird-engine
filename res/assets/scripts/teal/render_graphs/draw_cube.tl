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

-- Draws the static cube with forward Cook-Torrance shading and the shadow of one spot light. See
-- ApplicationVulkanDrawCubeTest in tests/executable/function/render/vulkan_draw_cube_test.cpp.
-- Imports: `swap_chain`, which the renderer supplies, and the application's `camera_info`, `spot_light_info`,
-- `shadow_light_info`, and `spot_light_count` uniform buffers.

local type wbe = require("wbe")
local render_graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.ShadowedMeshRecipeContext, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local graph = render_graph.new()
    local swap_chain = graph:import_resource("swap_chain")
    local camera_info = graph:import_resource("camera_info")
    local spot_light_info = graph:import_resource("spot_light_info")
    local shadow_light_info = graph:import_resource("shadow_light_info")
    local spot_light_count = graph:import_resource("spot_light_count")
    local shadow_extent: wbe.RendExtent2DParamsVK = { width = context.shadow_extent.width, height = context.shadow_extent.height }

    local mesh = graph:register_mesh("static_mesh", { mesh = "model_cube_static.mesh" })
    local shadow_pipeline = graph:register_graphics_pipeline("draw_cube_shadow_pipeline", {
        pipeline = "shadowmap_spot",
        color_formats = { context.shadow_color_format },
        depth_format = context.shadow_depth_format,
    })
    local main_pipeline = graph:register_graphics_pipeline("draw_cube_main_pipeline", {
        pipeline = "forward_cook_torrance_pbr",
        color_formats = { context.swap_chain_format },
    })
    local shadow_depth = graph:add_image("shadowmap_depth_image", "image_depth_target", context.shadow_depth_format,
        shadow_extent):view("shadowmap_depth")
    local shadow_color = graph:add_image("shadowmap_color_image", "image_render_target", context.shadow_color_format,
        shadow_extent):view("shadowmap_color")
    local shadow_sampler = graph:add_clamp_to_edge_sampler("shadow_sampler")

    local shadow = graph:draw_mesh("run_pipeline_shadowmap", {
        mesh = mesh,
        pipeline = { default = shadow_pipeline },
        parts = { "opaque" },
        render_extent = shadow_extent,
        clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } }, { depth = 0.0 } },
    }, { uses = { shadow_light_info } }):color_target(0, shadow_color):depth_target(shadow_depth)
    -- The main pass samples the shadow depth through its reflected "shadowmap" resource.
    local main = graph:draw_mesh("run_pipeline", {
        mesh = mesh,
        pipeline = { default = main_pipeline },
        parts = { "opaque" },
        clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } } },
    }, {
        bindings = { shadowmap = shadow_depth },
        uses = { camera_info, spot_light_info, spot_light_count, shadow_sampler },
    }):color_target(0, swap_chain)
    shadow:connect(main, shadow_depth)
    return graph:build()
end

return exports
