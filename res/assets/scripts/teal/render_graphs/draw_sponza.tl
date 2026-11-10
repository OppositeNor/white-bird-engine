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
local render_graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.ShadowedMeshRecipeContext, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local graph = render_graph.new()
    local swap_chain = render_graph.import_resource("swap_chain")
    local camera_info = render_graph.import_resource("camera_info")
    local gbuffer_camera_info = render_graph.import_resource("gbuffer_camera_info")
    local spot_light_info = render_graph.import_resource("spot_light_info")
    local shadow_light_info = render_graph.import_resource("shadow_light_info")
    local spot_light_count = render_graph.import_resource("spot_light_count")
    local light_billboard_info = render_graph.import_resource("light_billboard_info")
    -- The billboard texture is registered on the graph, which binds the registered view for its import.
    local light_billboard_texture = graph:register_image("light_billboard_texture", { texture = "spot_light_billboard" })
    local shadow_extent: wbe.RendExtent2DParamsVK = { width = context.shadow_extent.width, height = context.shadow_extent.height }

    local mesh = graph:register_mesh("static_mesh", { mesh = "sponza.mesh" })
    local function pipeline(key: string, pipeline_key: string, color_format: wbe.ImageFormatVK,
        depth_format?: wbe.ImageFormatVK): render_graph.Resource
        return graph:register_graphics_pipeline(key, {
            pipeline = pipeline_key,
            color_formats = { color_format },
            depth_format = depth_format,
        })
    end
    local shadow_opaque_pipeline = pipeline("forward_sponza_shadow_opaque", "shadowmap_spot", context.shadow_color_format,
        context.shadow_depth_format)
    local shadow_masked_pipeline = pipeline("forward_sponza_shadow_masked", "shadowmap_spot_masked",
        context.shadow_color_format, context.shadow_depth_format)
    local main_opaque_pipeline = pipeline("forward_sponza_main_opaque", "forward_cook_torrance_pbr", context.swap_chain_format,
        context.depth_format)
    local main_masked_pipeline = pipeline("forward_sponza_main_masked", "forward_cook_torrance_pbr_masked",
        context.swap_chain_format, context.depth_format)
    local light_billboard_pipeline = pipeline("light_billboard_pipeline", "billboard_face_camera_pipeline",
        context.swap_chain_format)

    local shadow_depth = render_graph.image("shadowmap_depth_image", "image_depth_target", context.shadow_depth_format,
        shadow_extent):view("shadowmap_depth")
    local shadow_color = render_graph.image("shadowmap_color_image", "image_render_target", context.shadow_color_format,
        shadow_extent):view("shadowmap_color")
    local depth = render_graph.image("draw_sponza_depth_image", "image_depth_target", context.depth_format):view("sponza_depth")
    local shadow_sampler = render_graph.clamp_to_edge_sampler("shadow_sampler")
    local light_billboard_sampler = render_graph.clamp_to_edge_sampler("light_billboard_sampler")

    -- The masked passes load what the opaque pass of their targets wrote.
    local function draw(identifier: string, pipeline_handle: render_graph.Resource, part: wbe.MeshPartVK, load: boolean,
        extent?: wbe.RendExtent2DParamsVK, bindings?: {string:render_graph.Resource},
        uses?: {render_graph.Resource}): render_graph.Task
        local load_op: wbe.AttachmentLoadOpVK = load and "load" or "clear"
        return graph:draw_mesh(identifier, {
            mesh = mesh,
            pipeline = { default = pipeline_handle },
            parts = { part },
            render_extent = extent,
            clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } }, { depth = 0.0 } },
            color_load_op = load_op,
            depth_load_op = load_op,
        }, { bindings = bindings, uses = uses })
    end
    local shadow_opaque = draw("run_shadowmap_opaque", shadow_opaque_pipeline, "opaque", false, shadow_extent, nil,
        { shadow_light_info }):color_target(0, shadow_color):depth_target(shadow_depth)
    local shadow_masked = draw("run_shadowmap_masked", shadow_masked_pipeline, "masked", true, shadow_extent, nil,
        { shadow_light_info }):color_target(0, shadow_color):depth_target(shadow_depth)
    -- The main passes sample the shadow depth through their reflected "shadowmap" resource.
    local main_uses = { camera_info, spot_light_info, spot_light_count, shadow_sampler }
    local main_opaque = draw("run_main_opaque", main_opaque_pipeline, "opaque", false, nil, { shadowmap = shadow_depth },
        main_uses):color_target(0, swap_chain):depth_target(depth)
    local main_masked = draw("run_main_masked", main_masked_pipeline, "masked", true, nil, { shadowmap = shadow_depth },
        main_uses):color_target(0, swap_chain):depth_target(depth)
    local light_billboard_params: render_graph.RTParamsRunGraphicsPipelineVK = {
        pipeline = light_billboard_pipeline,
        color_load_op = "load",
        draw_vertex_count = 6,
    }
    local light_billboard = graph:draw("run_light_billboard", light_billboard_params, {
        uses = { gbuffer_camera_info, light_billboard_info, light_billboard_texture, light_billboard_sampler },
    }):color_target(0, swap_chain)

    shadow_opaque:connect(shadow_masked, shadow_color)
    shadow_opaque:connect(shadow_masked, shadow_depth)
    shadow_masked:connect(main_opaque, shadow_depth)
    shadow_masked:connect(main_masked, shadow_depth)
    main_opaque:connect(main_masked, swap_chain)
    main_opaque:connect(main_masked, depth)
    main_masked:connect(light_billboard, swap_chain)
    return graph:build()
end

return exports
