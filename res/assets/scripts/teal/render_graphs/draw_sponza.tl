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

-- Draws Sponza with forward Cook-Torrance shading, the shadow of one spot light, and a billboard at the light. See
-- ApplicationVulkanDrawSponzaTest in tests/executable/function/render/vulkan_draw_sponza_test.cpp.
-- Imports: `swap_chain`, which the renderer supplies, and the application's `camera_info` and `gbuffer_camera_info` (one
-- per-frame camera buffer), `spot_light_info` and `shadow_light_info` (one per-frame light buffer), `spot_light_count`, and
-- `light_billboard_info` buffers.

local type wbe = require("wbe")
local graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.ShadowedMeshRecipeContext, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local swap_chain = graph.import("swap_chain")
    local shadow_extent: wbe.RendExtent2DParamsVK = { width = context.shadow_extent.width, height = context.shadow_extent.height }

    local mesh = graph.mesh("static_mesh", { mesh = "sponza.mesh" })
    local function pipeline(key: string, pipeline_key: string, color_format: wbe.ImageFormatVK,
        depth_format?: wbe.ImageFormatVK): graph.Resource
        return graph.graphics_pipeline(key, {
            pipeline = pipeline_key,
            color_formats = { color_format },
            depth_format = depth_format,
        })
    end

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
    local depth = graph.image({ name = "sponza_depth", usage = "image_depth_target", format = context.depth_format }):view()

    -- The masked passes load what the opaque pass of their targets wrote.
    local function draw(name: string, pipeline_handle: graph.Resource, part: wbe.MeshPartVK, load: boolean,
        extent: wbe.RendExtent2DParamsVK, bindings: {string:graph.Resource}): graph.Task
        local load_op: wbe.AttachmentLoadOpVK = load and "load" or "clear"
        return graph.draw_mesh({
            mesh = mesh,
            pipeline = { default = pipeline_handle },
            parts = { part },
            render_extent = extent,
            clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } }, { depth = 0.0 } },
            color_load_op = load_op,
            depth_load_op = load_op,
        }, { name = name, bindings = bindings })
    end
    local shadow_bindings = { shadow_light_info = graph.import("shadow_light_info") }
    local shadow_opaque = draw("draw_shadow_map_opaque", pipeline("forward_sponza_shadow_opaque", "shadowmap_spot",
        context.shadow_color_format, context.shadow_depth_format), "opaque", false, shadow_extent, shadow_bindings)
        :color_target(0, shadow_color):depth_target(shadow_depth)
    local shadow_masked = draw("draw_shadow_map_masked", pipeline("forward_sponza_shadow_masked", "shadowmap_spot_masked",
        context.shadow_color_format, context.shadow_depth_format), "masked", true, shadow_extent, shadow_bindings)
        :color_target(0, shadow_color):depth_target(shadow_depth)
    local main_bindings = {
        shadowmap = shadow_depth,
        shadow_sampler = graph.clamp_to_edge_sampler({ name = "shadow_sampler" }),
        camera_info = graph.import("camera_info"),
        spot_light_info = graph.import("spot_light_info"),
        spot_light_count = graph.import("spot_light_count"),
    }
    local main_opaque = draw("draw_main_opaque", pipeline("forward_sponza_main_opaque", "forward_cook_torrance_pbr",
        context.swap_chain_format, context.depth_format), "opaque", false, nil, main_bindings)
        :color_target(0, swap_chain):depth_target(depth)
    local main_masked = draw("draw_main_masked", pipeline("forward_sponza_main_masked", "forward_cook_torrance_pbr_masked",
        context.swap_chain_format, context.depth_format), "masked", true, nil, main_bindings)
        :color_target(0, swap_chain):depth_target(depth)
    local light_billboard_params: graph.RTParamsRunGraphicsPipelineVK = {
        pipeline = pipeline("light_billboard_pipeline", "billboard_face_camera_pipeline", context.swap_chain_format),
        color_load_op = "load",
        draw_vertex_count = 6,
    }
    local light_billboard = graph.draw(light_billboard_params, {
        name = "draw_light_billboard",
        bindings = {
            gbuffer_camera_info = graph.import("gbuffer_camera_info"),
            light_billboard_info = graph.import("light_billboard_info"),
            -- The billboard texture is registered on the graph, which binds the registered view for its import.
            light_billboard_texture = graph.registered_image("light_billboard_texture", { texture = "spot_light_billboard" }),
            light_billboard_sampler = graph.clamp_to_edge_sampler({ name = "light_billboard_sampler" }),
        },
    }):color_target(0, swap_chain)

    shadow_opaque:connect(shadow_masked, { shadow_color, shadow_depth })
    shadow_masked:connect_all({ main_opaque, main_masked }, shadow_depth)
    main_opaque:connect(main_masked, { swap_chain, depth })
    main_masked:connect(light_billboard, swap_chain)
    return graph.build({ tasks = { shadow_opaque, shadow_masked, main_opaque, main_masked, light_billboard } })
end

return exports
