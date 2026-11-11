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
local graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.ShadowedMeshRecipeContext, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local shadow_extent: wbe.RendExtent2DParamsVK = { width = context.shadow_extent.width, height = context.shadow_extent.height }
    local mesh = graph.mesh("static_mesh", { mesh = "model_cube_static.mesh" })
    local shadow_depth = graph.image({
        name = "shadowmap_depth",
        usage = "image_depth_target",
        format = context.shadow_depth_format,
        extent = shadow_extent,
    }):view()
    local shadow_color = graph.image({
        name = "shadowmap_color",
        usage = "image_render_target",
        format = context.shadow_color_format,
        extent = shadow_extent,
    }):view()

    local shadow = graph.draw_mesh({
        mesh = mesh,
        pipeline = { default = graph.graphics_pipeline("draw_cube_shadow_pipeline", {
            pipeline = "shadowmap_spot",
            color_formats = { context.shadow_color_format },
            depth_format = context.shadow_depth_format,
        }) },
        parts = { "opaque" },
        render_extent = shadow_extent,
        clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } }, { depth = 0.0 } },
    }, {
        name = "draw_shadow_map",
        bindings = { shadow_light_info = graph.import("shadow_light_info") },
    }):color_target(0, shadow_color):depth_target(shadow_depth)
    local main = graph.draw_mesh({
        mesh = mesh,
        pipeline = { default = graph.graphics_pipeline("draw_cube_main_pipeline", {
            pipeline = "forward_cook_torrance_pbr",
            color_formats = { context.swap_chain_format },
        }) },
        parts = { "opaque" },
        clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } } },
    }, {
        name = "draw_main",
        bindings = {
            shadowmap = shadow_depth,
            shadow_sampler = graph.clamp_to_edge_sampler({ name = "shadow_sampler" }),
            camera_info = graph.import("camera_info"),
            spot_light_info = graph.import("spot_light_info"),
            spot_light_count = graph.import("spot_light_count"),
        },
    }):color_target(0, graph.import("swap_chain"))
    shadow:connect(main, shadow_depth)
    return graph.build({ tasks = { shadow, main } })
end

return exports
