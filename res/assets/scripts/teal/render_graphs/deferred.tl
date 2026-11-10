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

-- The deferred PBR test's render graph: per-light shadow and reflective shadow maps, a G-buffer, direct and indirect
-- lighting, post-processing, bloom, presentation, and overlays. See ApplicationVulkanDeferredPBRTest in
-- tests/executable/function/render/deferred_test/.
--
-- The application's bindings import every pass it builds in C++ as a task (the acceleration structure build, the frame
-- sequence acquisition, and ImGui), its buffers, its acceleration structure, and its dynamic mesh entities. The recipe
-- registers the meshes its draws import ("static_mesh" and "dynamic_mesh", from the descriptions in the context), its own
-- pipelines, and the billboard texture.
-- The bindings supply a wbe.DeferredRecipeContext, whose voxel and post_processing entries are the RecipeFeatureParams-
-- derived parameters of the feature modules it selects: a VoxelFeature and a PostProcessingFeature. Each render graph
-- definition sets the wbe.DeferredRecipeParameters.

local type wbe = require("wbe")
local type voxel = require("render_graph_utils.features.voxel")
local math_utils = require("utils.math")
local render_graph = require("render_graph_utils.render_graph_utils")

local ceil_div = math_utils.ceil_div


-- VK_REMAINING_MIP_LEVELS: a view of every mip level from the base level on.
local REMAINING_MIP_LEVELS = 4294967295
-- One workgroup per 16x16 tile of the lit images, which have the swap chain extent.
local SCREEN_TILE_SIZE = 16
-- One invocation per shadow-map line in the summed-area-table passes.
local SUMMED_AREA_TABLE_WORKGROUP_SIZE = 64
-- One workgroup per 16x16 tile of the bloom images.
local BLOOM_TILE_SIZE = 16

local record VoxelFeature
    -- The structure as a subgraph task, whose overlays draw on the target's color and depth.
    subgraph: function(identifier: string, parameters: any, target: voxel.Target): render_graph.Task
    -- Whether the subgraph injects radiance from the target's reflective-shadow-map slots, so it follows the lights' shadow
    -- passes.
    injects_radiance: boolean
end

local record PostProcessingTarget
    color_view: render_graph.Resource
    width: integer
    height: integer
end

local record PostProcessingFeature
    -- The effect as a subgraph task, which modifies the target's color view in place.
    subgraph: function(identifier: string, parameters: any, target: PostProcessingTarget): render_graph.Task
end

local function light_id(base: string, light_data: integer): string
    return base .. "_" .. light_data
end

-- The ids of the feature subgraphs.
local VOXEL = "voxel"
local POST_PROCESSING = "post_processing"

local CLEAR_BLACK: wbe.RTParamsClearValueVK = { color = { 0.0, 0.0, 0.0, 0.0 } }
local CLEAR_DEPTH: wbe.RTParamsClearValueVK = { depth = 0.0 }
-- The moments clear to the farthest depth, so unrendered texels never shadow.
local CLEAR_MOMENTS: wbe.RTParamsClearValueVK = { color = { 1.0, 1.0, 0.0, 0.0 } }

-- The recipe under construction, shared by the build steps. The handle map lets the per-light passes share their
-- resources; the graph discovers declarations only from task references and connections.
local record LightTasks
    run_shadow_opaque: render_graph.Task
    run_shadow_double_sided: render_graph.Task
    dynamic_shadow: render_graph.Task
    dynamic_shadow_second: render_graph.Task
    build_shadow_sat: render_graph.Task
    generate_shadow_mipmaps: render_graph.Task
    run_light_billboard: render_graph.Task
end

local record Tasks
    build_mesh_acceleration_structure: render_graph.Task
    acquire_frame_sequence: render_graph.Task
    draw_imgui: render_graph.Task
    voxel: render_graph.Task
    run_gbuffer: render_graph.Task
    run_gbuffer_double_sided: render_graph.Task
    dynamic_gbuffer: render_graph.Task
    dynamic_gbuffer_second: render_graph.Task
    run_direct_lighting: render_graph.Task
    indirect_lighting: render_graph.Task
    combine_lighting: render_graph.Task
    post_processing: render_graph.Task
    extract_bloom: render_graph.Task
    blur_bloom: render_graph.Task
    acquire_bloom: render_graph.Task
    present_result_image: render_graph.Task
    present_frame_sequence: render_graph.Task
end

local record Pipelines
    shadow_opaque: render_graph.Resource
    shadow_double_sided: render_graph.Resource
    shadow_dynamic: render_graph.Resource
    gbuffer_opaque: render_graph.Resource
    gbuffer_double_sided: render_graph.Resource
    gbuffer_dynamic: render_graph.Resource
    shadow_sat_horizontal: render_graph.Resource
    shadow_sat_vertical: render_graph.Resource
    direct_lighting: render_graph.Resource
    direct_lighting_sat: render_graph.Resource
    indirect_lighting: render_graph.Resource
    indirect_lighting_debug_view: render_graph.Resource
    combine_lighting: render_graph.Resource
    extract_bloom: render_graph.Resource
    blur_ping: render_graph.Resource
    blur_pong: render_graph.Resource
    present: render_graph.Resource
    light_billboard: render_graph.Resource
end

-- The pipelines the passes draw and dispatch with, each registered on the render graph from its pipeline or shader
-- resource when a task names it.
local function make_pipelines(recipe_context: wbe.DeferredRecipeContext): Pipelines
    local formats = recipe_context.formats
    local function graphics(key: string, pipeline_key: string, color_formats: {wbe.ImageFormatVK},
        depth_format?: wbe.ImageFormatVK): render_graph.Resource
        return render_graph.graphics_pipeline(key, {
            pipeline = pipeline_key,
            color_formats = color_formats,
            depth_format = depth_format,
        })
    end
    local shadow_formats = { formats.rsm_flux, formats.rsm_normal, formats.shadow_moments }
    local gbuffer_formats = { formats.gbuffer_albedo, formats.gbuffer_normal, formats.gbuffer_rma }
    local shader_pipeline = render_graph.shader_compute_pipeline
    return {
        shadow_opaque = graphics("deferred_shadow_opaque", "rsm_spot", shadow_formats, formats.shadowmap_depth),
        shadow_double_sided = graphics("deferred_shadow_double_sided", "rsm_spot_opaque_double_sided", shadow_formats,
            formats.shadowmap_depth),
        -- Placed meshes use the WBE_MESH_TRANSFORM shader variants, which also bind the entity's mesh transform.
        shadow_dynamic = graphics("deferred_shadow_dynamic", "rsm_spot_dynamic", shadow_formats, formats.shadowmap_depth),
        gbuffer_opaque = graphics("deferred_gbuffer_opaque", "deferred_gbuffer", gbuffer_formats, formats.gbuffer_depth),
        gbuffer_double_sided = graphics("deferred_gbuffer_double_sided", "deferred_gbuffer_opaque_double_sided",
            gbuffer_formats, formats.gbuffer_depth),
        gbuffer_dynamic = graphics("deferred_gbuffer_dynamic", "deferred_gbuffer_dynamic", gbuffer_formats,
            formats.gbuffer_depth),
        shadow_sat_horizontal = shader_pipeline("shadow_moments_summed_area_table_horizontal_shader"),
        shadow_sat_vertical = shader_pipeline("shadow_moments_summed_area_table_vertical_shader"),
        direct_lighting = shader_pipeline("direct_lighting_shader"),
        direct_lighting_sat = shader_pipeline("direct_lighting_summed_area_table_shader"),
        indirect_lighting = shader_pipeline("indirect_lighting_rsm_shader"),
        indirect_lighting_debug_view = shader_pipeline("indirect_lighting_rsm_debug_view_shader"),
        combine_lighting = shader_pipeline("combine_lighting_shader"),
        extract_bloom = shader_pipeline("bloom_shader_extract"),
        blur_ping = shader_pipeline("kawase_blur_pong_write_shader"),
        blur_pong = shader_pipeline("kawase_blur_ping_write_shader"),
        present = graphics("present_post_processing", "present_post_processing", { recipe_context.swap_chain_format }),
        light_billboard = graphics("deferred_light_billboard_pipeline", "billboard_face_camera_depth_tested_pipeline",
            { recipe_context.swap_chain_format }, formats.gbuffer_depth),
    }
end

local record Builder
    context: wbe.DeferredRecipeContext
    parameters: wbe.DeferredRecipeParameters
    shadow_extent: wbe.RendExtent2DParamsVK
    bloom_extent: wbe.RendExtent2DParamsVK
    imports: {string}
    resources: {string:render_graph.Resource}
    static_mesh: render_graph.Resource
    dynamic_mesh: render_graph.Resource
    pipelines: Pipelines
    graph: render_graph.Graph
    tasks: Tasks
    light_tasks: {LightTasks}
    voxel_injects_radiance: boolean
end

-- The reflective-shadow-map resources of every slot of the lighting shaders' lights[] array, by task-local id; slots past
-- the light count reuse light zero, so that every reflected descriptor binds. Only mip level zero of the moments is loaded,
-- so the mip-chain slot binds the base moments view, which the summed-area-table graph orders without a mip generation pass.
local function rsm_slot_resources(graph_builder: Builder): {string:render_graph.Resource}
    local resources = graph_builder.resources
    local context = graph_builder.context
    local slots: {string:render_graph.Resource} = {}
    for slot = 0, context.max_spot_light_info_count - 1 do
        local light = slot < context.spot_light_count and slot or 0
        slots["shadow_moments_sampled[" .. slot .. "]"] = resources[light_id("shadow_moments", light)]
        slots["shadowmap_color[" .. slot .. "]"] = resources[light_id("shadowmap_color", light)]
        slots["rsm_normal[" .. slot .. "]"] = resources[light_id("rsm_normal", light)]
    end
    return slots
end

-- Shadow and reflective-shadow-map targets of every light. Every light shares one shadow sampler.
local function create_light_resources(graph_builder: Builder)
    local graph = graph_builder.graph
    local context = graph_builder.context
    local formats = context.formats
    local shadow_extent = graph_builder.shadow_extent
    local resources = graph_builder.resources
    for light = 0, context.spot_light_count - 1 do
        local light_info = light_id("shadow_light_info", light)
        local billboard_info = light_id("light_billboard_info", light)
        graph_builder.imports[#graph_builder.imports + 1] = light_info
        graph_builder.imports[#graph_builder.imports + 1] = billboard_info
        resources[light_info] = graph:import_resource(light_info)
        resources[billboard_info] = graph:import_resource(billboard_info)
        local depth = graph:add_image(light_id("shadowmap_depth_image", light), "image_depth_target", formats.shadowmap_depth, shadow_extent)
        local color = graph:add_image(light_id("shadowmap_color_image", light), "image_render_target", formats.rsm_flux, shadow_extent)
        local normal = graph:add_image(light_id("rsm_normal_image", light), "image_render_target", formats.rsm_normal, shadow_extent)
        local moments = graph:add_image(light_id("shadow_moments_image", light), "image_render_target", formats.shadow_moments,
            shadow_extent, { "transfer_src", "transfer_dst" }, context.shadow_mipmap_level_count)
        local horizontal = graph:add_image(light_id("shadow_moments_sat_horizontal_image", light), "image_read_write",
            formats.shadow_moments, shadow_extent)
        local summed = graph:add_image(light_id("shadow_moments_sat_image", light), "image_read_write", formats.shadow_moments,
            shadow_extent)
        resources[light_id("shadowmap_depth", light)] = depth:view(light_id("shadowmap_depth", light))
        resources[light_id("shadowmap_color", light)] = color:view(light_id("shadowmap_color", light))
        resources[light_id("rsm_normal", light)] = normal:view(light_id("rsm_normal", light))
        resources[light_id("shadow_moments", light)] = moments:view(light_id("shadow_moments", light))
        -- Direct lighting samples the complete mip chain through a second view of the moments image.
        resources[light_id("shadow_moments_sampled", light)] = moments:view(light_id("shadow_moments_sampled", light),
            { mip_level_count = REMAINING_MIP_LEVELS })
        resources[light_id("shadow_sat_horizontal", light)] = horizontal:view(light_id("shadow_sat_horizontal", light))
        resources[light_id("shadow_sat", light)] = summed:view(light_id("shadow_sat", light))
    end
    resources.shadow_sampler = graph:add_clamp_to_edge_sampler("shadow_sampler",
        { min_lod = 0.0, max_lod = context.shadow_mipmap_level_count - 1.0 })
end

-- The G-buffer targets and the lighting and post-processing outputs. Views retain their source image handles.
local function create_screen_resources(graph_builder: Builder)
    local graph = graph_builder.graph
    local context = graph_builder.context
    local formats = context.formats
    local bloom_extent = graph_builder.bloom_extent
    local resources = graph_builder.resources
    resources.gbuffer_albedo = graph:add_image("gbuffer_albedo_image", "image_render_target", formats.gbuffer_albedo):view("gbuffer_albedo")
    resources.gbuffer_normal = graph:add_image("gbuffer_normal_image", "image_render_target", formats.gbuffer_normal):view("gbuffer_normal")
    resources.gbuffer_rma = graph:add_image("gbuffer_rma_image", "image_render_target", formats.gbuffer_rma):view("gbuffer_rma")
    resources.gbuffer_depth = graph:add_image("gbuffer_depth_image", "image_depth_target", formats.gbuffer_depth):view("gbuffer_depth")
    resources.gbuffer_sampler = graph:add_clamp_to_edge_sampler("gbuffer_sampler", { min_filter = "nearest", mag_filter = "nearest" })

    -- The bloom images keep the extent of the initial swap chain, because the bloom readback buffer is sized by it.
    resources.direct_lighting = graph:add_image("direct_lighting_image", "image_read_write", formats.lighting_output):view("direct_lighting")
    resources.indirect_lighting = graph:add_image("indirect_lighting_image", "image_read_write", formats.lighting_output):view("indirect_lighting")
    resources.result_image = graph:add_image("result_color_image", "image_read_write", formats.lighting_output):view("result_image")
    resources.bloom_ping = graph:add_image("bloom_ping_image", "image_read_write", formats.lighting_output,
        bloom_extent, { "transfer_src" }):view("bloom_ping")
    resources.bloom_pong = graph:add_image("bloom_pong_image", "image_read_write", formats.lighting_output,
        bloom_extent):view("bloom_pong")
    -- The present pipeline renders in the swap-chain format, so the sequence matches the presented image.
    resources.frame_sequence = graph:add_image("frame_sequence_image", "image_render_target", context.swap_chain_format):view("frame_sequence")
    resources.bloom_sampler = graph:add_clamp_to_edge_sampler("bloom_sampler")
    resources.light_billboard_sampler = graph:add_clamp_to_edge_sampler("light_billboard_sampler")
end

-- Tasks the application builds in C++.
local function import_application_tasks(graph_builder: Builder)
    if graph_builder.context.has_mesh_acceleration_structure then
        graph_builder.tasks.build_mesh_acceleration_structure = graph_builder.graph:import_task("build_mesh_acceleration_structure")
    end
    graph_builder.tasks.acquire_frame_sequence = graph_builder.graph:import_task("acquire_frame_sequence")
    graph_builder.tasks.draw_imgui = graph_builder.graph:import_task("draw_imgui")
end

-- The voxel structure, a subgraph whose overlays draw over the lit scene.
local function add_voxel(graph_builder: Builder)
    local resources = graph_builder.resources
    local context = graph_builder.context
    local voxel_params = context.voxel as wbe.RecipeFeatureParams
    local voxel_feature = require(voxel_params.module) as VoxelFeature
    local rsm_slot_ids: {string:string} = {}
    for slot, resource in pairs(rsm_slot_resources(graph_builder)) do
        rsm_slot_ids[slot] = resource.id
    end
    graph_builder.voxel_injects_radiance = voxel_feature.injects_radiance == true
    graph_builder.tasks.voxel = graph_builder.graph:add(voxel_feature.subgraph(VOXEL, voxel_params, {
        dynamic_mesh_transform_ids = context.dynamic_mesh_transform_ids,
        debug_view_toggles = context.voxel_debug_view_toggles,
        color_target = resources.swap_chain,
        depth_target = resources.gbuffer_depth,
        color_format = graph_builder.context.swap_chain_format,
        depth_format = graph_builder.context.formats.gbuffer_depth,
        rsm_slot_ids = rsm_slot_ids,
    }))
end

-- Mesh draws. The double-sided and dynamic passes load what the previous pass of their targets wrote.
local function add_mesh_draws(graph_builder: Builder)
    local graph = graph_builder.graph
    local resources = graph_builder.resources
    local dynamic_transforms = graph_builder.context.dynamic_mesh_transform_ids
    if #dynamic_transforms ~= 2 then
        error("deferred: the dynamic mesh draws two entities, but " .. #dynamic_transforms .. " transforms are supplied.")
    end
    local shadow_extent = graph_builder.shadow_extent
    for light = 0, graph_builder.context.spot_light_count - 1 do
        local tasks: LightTasks = {}
        graph_builder.light_tasks[light + 1] = tasks
        local shadow_color = resources[light_id("shadowmap_color", light)]
        local rsm_normal = resources[light_id("rsm_normal", light)]
        local shadow_moments = resources[light_id("shadow_moments", light)]
        local shadow_depth = resources[light_id("shadowmap_depth", light)]
        local clear_values = { CLEAR_BLACK, CLEAR_BLACK, CLEAR_MOMENTS, CLEAR_DEPTH }
        local light_bindings: {string:render_graph.Resource} = {
            shadow_light_info = resources[light_id("shadow_light_info", light)],
        }

        tasks.run_shadow_opaque = graph:draw_mesh(light_id("run_shadow_opaque", light), {
            mesh = graph_builder.static_mesh,
            pipeline = { default = graph_builder.pipelines.shadow_opaque },
            parts = { "opaque" },
            render_extent = shadow_extent,
            clear_values = clear_values,
            color_load_op = "clear",
            depth_load_op = "clear",
        }, { bindings = light_bindings }):depth_target(shadow_depth)
        :color_target(0, shadow_color)
        :color_target(1, rsm_normal)
        :color_target(2, shadow_moments)

        tasks.run_shadow_double_sided = graph:draw_mesh(light_id("run_shadow_double_sided", light), {
            mesh = graph_builder.static_mesh,
            pipeline = { default = graph_builder.pipelines.shadow_double_sided },
            parts = { "double_sided" },
            render_extent = shadow_extent,
            clear_values = clear_values,
            color_load_op = "load",
            depth_load_op = "load",
        }, { bindings = light_bindings }):depth_target(shadow_depth)
        :color_target(0, shadow_color)
        :color_target(1, rsm_normal)
        :color_target(2, shadow_moments)

        tasks.dynamic_shadow = graph:draw_mesh(light_id("dynamic_shadow", light), {
            mesh = graph_builder.dynamic_mesh,
            pipeline = { default = graph_builder.pipelines.shadow_dynamic },
            parts = { "opaque" },
            transform = graph:reference(dynamic_transforms[1]),
            render_extent = shadow_extent,
            clear_values = {},
            color_load_op = "load",
            depth_load_op = "load",
        }, { bindings = light_bindings }):depth_target(shadow_depth)
        :color_target(0, shadow_color)
        :color_target(1, rsm_normal)
        :color_target(2, shadow_moments)

        tasks.dynamic_shadow_second = graph:draw_mesh(light_id("dynamic_shadow_second", light), {
            mesh = graph_builder.dynamic_mesh,
            pipeline = { default = graph_builder.pipelines.shadow_dynamic },
            parts = { "opaque" },
            transform = graph:reference(dynamic_transforms[2]),
            render_extent = shadow_extent,
            clear_values = {},
            color_load_op = "load",
            depth_load_op = "load",
        }, { bindings = light_bindings }):depth_target(shadow_depth)
        :color_target(0, shadow_color)
        :color_target(1, rsm_normal)
        :color_target(2, shadow_moments)
    end

    local gbuffer_clear_values = { CLEAR_BLACK, CLEAR_BLACK, CLEAR_BLACK, CLEAR_DEPTH }

    graph_builder.tasks.run_gbuffer = graph:draw_mesh("run_gbuffer", {
        mesh = graph_builder.static_mesh,
        pipeline = { default = graph_builder.pipelines.gbuffer_opaque },
        parts = { "opaque" },
        clear_values = gbuffer_clear_values,
        color_load_op = "clear",
        depth_load_op = "clear",
    })
    :depth_target(resources.gbuffer_depth)
    :color_target(0, resources.gbuffer_albedo)
    :color_target(1, resources.gbuffer_normal)
    :color_target(2, resources.gbuffer_rma)

    graph_builder.tasks.run_gbuffer_double_sided = graph:draw_mesh("run_gbuffer_double_sided", {
        mesh = graph_builder.static_mesh,
        pipeline = { default = graph_builder.pipelines.gbuffer_double_sided },
        parts = { "double_sided" },
        clear_values = gbuffer_clear_values,
        color_load_op = "load",
        depth_load_op = "load",
    })
    :depth_target(resources.gbuffer_depth)
    :color_target(0, resources.gbuffer_albedo)
    :color_target(1, resources.gbuffer_normal)
    :color_target(2, resources.gbuffer_rma)

    graph_builder.tasks.dynamic_gbuffer = graph:draw_mesh("dynamic_gbuffer", {
        mesh = graph_builder.dynamic_mesh,
        pipeline = { default = graph_builder.pipelines.gbuffer_dynamic },
        parts = { "opaque" },
        transform = graph:reference(dynamic_transforms[1]),
        clear_values = {},
        color_load_op = "load",
        depth_load_op = "load",
    })
    :depth_target(resources.gbuffer_depth)
    :color_target(0, resources.gbuffer_albedo)
    :color_target(1, resources.gbuffer_normal)
    :color_target(2, resources.gbuffer_rma)

    graph_builder.tasks.dynamic_gbuffer_second = graph:draw_mesh("dynamic_gbuffer_second", {
        mesh = graph_builder.dynamic_mesh,
        pipeline = { default = graph_builder.pipelines.gbuffer_dynamic },
        parts = { "opaque" },
        transform = graph:reference(dynamic_transforms[2]),
        clear_values = {},
        color_load_op = "load",
        depth_load_op = "load",
    })
    :depth_target(resources.gbuffer_depth)
    :color_target(0, resources.gbuffer_albedo)
    :color_target(1, resources.gbuffer_normal)
    :color_target(2, resources.gbuffer_rma)
end

-- Shadow filtering of every light.
local function add_shadow_filtering(graph_builder: Builder)
    local resources = graph_builder.resources
    local context = graph_builder.context
    local shadow = context.shadow_extent
    for light = 0, context.spot_light_count - 1 do
        local tasks = graph_builder.light_tasks[light + 1]
        if graph_builder.parameters.summed_area_table then
            tasks.build_shadow_sat = graph_builder.graph:compute_sequential(light_id("build_shadow_sat", light), {
                queue_family = "compute_1",
                dispatches = {
                    {
                        pipeline = graph_builder.pipelines.shadow_sat_horizontal,
                        group_count_x = ceil_div(shadow.height, SUMMED_AREA_TABLE_WORKGROUP_SIZE)
                    },
                    {
                        pipeline = graph_builder.pipelines.shadow_sat_vertical,
                        group_count_x = ceil_div(shadow.width, SUMMED_AREA_TABLE_WORKGROUP_SIZE)
                    },
                },
            }, {
                    bindings = {
                        shadow_sat_horizontal = resources[light_id("shadow_sat_horizontal", light)],
                        shadow_sat = resources[light_id("shadow_sat", light)],
                    },
                })
        else
            local params: render_graph.RTParamsGenerateMipmapsVK = {
                source_view = resources[light_id("shadow_moments", light)],
                mip_chain_view = resources[light_id("shadow_moments_sampled", light)],
                base_extent = graph_builder.shadow_extent,
                mipmap_level_count = context.shadow_mipmap_level_count,
                format = context.formats.shadow_moments,
            }
            tasks.generate_shadow_mipmaps = graph_builder.graph:generate_mipmaps(light_id("generate_shadow_mipmaps", light), params)
        end
    end
end

-- Lighting. Every slot of the lighting shaders' lights[] array maps to a light's shadow resources; slots past the light
-- count reuse light zero, so that every reflected descriptor binds.
local function add_lighting(graph_builder: Builder)
    local resources = graph_builder.resources
    local context = graph_builder.context
    local summed_area_table = graph_builder.parameters.summed_area_table
    local light_count = context.spot_light_count
    local screen = context.swap_chain_extent
    local light_slots: {string:string}
    if summed_area_table then
        light_slots = { shadow_moments = "shadow_moments", shadow_sat = "shadow_sat", shadowmap_color = "shadowmap_color", rsm_normal = "rsm_normal" }
    else
        light_slots = { shadow_moments_sampled = "shadow_moments_sampled", shadowmap_color = "shadowmap_color", rsm_normal = "rsm_normal" }
    end
    local direct_lighting_bindings: {string:render_graph.Resource} = {
        gbuffer_sampler = resources.gbuffer_sampler,
        shadow_sampler = resources.shadow_sampler,
    }
    for slot = 0, context.max_spot_light_info_count - 1 do
        local light = slot < light_count and slot or 0
        for task_resource, graph_resource in pairs(light_slots) do
            direct_lighting_bindings[task_resource .. "[" .. slot .. "]"] = resources[light_id(graph_resource, light)]
        end
    end
    local screen_tiles_x = ceil_div(screen.width, SCREEN_TILE_SIZE)
    local screen_tiles_y = ceil_div(screen.height, SCREEN_TILE_SIZE)
    graph_builder.tasks.run_direct_lighting = graph_builder.graph:compute("run_direct_lighting", {
        pipeline = summed_area_table and graph_builder.pipelines.direct_lighting_sat or graph_builder.pipelines.direct_lighting,
        group_count_x = screen_tiles_x,
        group_count_y = screen_tiles_y,
        queue_family = "compute_1",
    }, { bindings = direct_lighting_bindings })
    -- Indirect lighting binds the reflective-shadow-map slots. It multiplexes the lit result and the voxel debug view,
    -- which the selection records while a voxel debug view is shown; the branches share its bindings.
    local indirect_lighting_bindings = rsm_slot_resources(graph_builder)
    indirect_lighting_bindings.gbuffer_sampler = resources.gbuffer_sampler
    indirect_lighting_bindings.shadow_sampler = resources.shadow_sampler
    local branch_graph = render_graph.new()
    local branches: {render_graph.Task} = {}
    for index, pipeline in ipairs({ graph_builder.pipelines.indirect_lighting, graph_builder.pipelines.indirect_lighting_debug_view }) do
        local params: render_graph.RTParamsRunComputePipelineVK =
        { pipeline = pipeline, group_count_x = screen_tiles_x, group_count_y = screen_tiles_y, queue_family = "compute_2" }
        branches[index] = branch_graph:compute("indirect_lighting_branch_" .. index, params)
    end
    graph_builder.tasks.indirect_lighting = graph_builder.graph:multiplex("run_indirect_lighting", branches, {
        select = context.voxel_debug_view_toggles.lighting,
        bindings = indirect_lighting_bindings,
    })
    graph_builder.tasks.combine_lighting = graph_builder.graph:compute("combine_lighting", {
        pipeline = graph_builder.pipelines.combine_lighting,
        group_count_x = screen_tiles_x,
        group_count_y = screen_tiles_y,
        queue_family = "compute_1",
    })
end

-- Post-processing, a subgraph, modifies the result image in place after composition. Returns the last task that writes
-- the result image.
local function add_post_processing(graph_builder: Builder): render_graph.Task
    local resources = graph_builder.resources
    local post_processing = graph_builder.context.post_processing as wbe.RecipeFeatureParams
    if post_processing == nil then
        return graph_builder.tasks.combine_lighting
    end
    local screen = graph_builder.context.swap_chain_extent
    local post_processing_feature = require(post_processing.module) as PostProcessingFeature
    graph_builder.tasks.post_processing = graph_builder.graph:add(post_processing_feature.subgraph(POST_PROCESSING, post_processing,
        { color_view = resources.result_image, width = screen.width, height = screen.height }))
    graph_builder.graph:chain(resources.result_image, { graph_builder.tasks.combine_lighting, graph_builder.tasks.post_processing })
    return graph_builder.tasks.post_processing
end

-- Bloom, presentation, and on-demand acquisition.
local function add_bloom_and_presentation(graph_builder: Builder)
    local resources = graph_builder.resources
    local context = graph_builder.context
    local bloom = context.bloom_extent
    local graph = graph_builder.graph
    local bloom_tiles_x = ceil_div(bloom.width, BLOOM_TILE_SIZE)
    local bloom_tiles_y = ceil_div(bloom.height, BLOOM_TILE_SIZE)
    graph_builder.tasks.extract_bloom = graph_builder.graph:compute("extract_bloom", {
        pipeline = graph_builder.pipelines.extract_bloom,
        group_count_x = bloom_tiles_x,
        group_count_y = bloom_tiles_y,
        queue_family = "compute_1",
    }, { bindings = { bloom_sampler = resources.bloom_sampler } })
    -- Starting from ping, even iterations blur ping into pong and odd iterations blur pong back into ping, so an even
    -- iteration count leaves the result in ping.
    graph_builder.tasks.blur_bloom = graph:ping_pong("blur_bloom", {
        ping_pipeline = graph_builder.pipelines.blur_ping,
        pong_pipeline = graph_builder.pipelines.blur_pong,
        iterations = graph_builder.parameters.bloom_blur_pass_count,
        group_count_x = bloom_tiles_x,
        group_count_y = bloom_tiles_y,
        queue_family = "compute_1",
    }, {
            bindings = {
                kawase_blur_ping = resources.bloom_ping,
                kawase_blur_pong = resources.bloom_pong,
                kawase_blur_sampler = resources.bloom_sampler
            },
        })
    local acquire_params: render_graph.RTParamsCPUInFlightAcquireVK = { image_view = resources.bloom_ping }
    graph_builder.tasks.acquire_bloom = graph:cpu_acquire_image("acquire_bloom", acquire_params, {
        trigger = { event = context.bloom_acquire_event },
    })
    local present_params: render_graph.RTParamsRunGraphicsPipelineVK = {
        pipeline = graph_builder.pipelines.present,
        clear_values = {
            {
                color = { 0.0, 0.0, 0.0, 1.0 }
            }
        }
    }
    graph_builder.tasks.present_result_image = graph:draw("present_result_image", present_params,
        { bindings = { bloom_sampler = resources.bloom_sampler } }):color_target(0, resources.swap_chain)
    graph_builder.tasks.present_frame_sequence = graph:draw("present_frame_sequence", present_params, {
        bindings = { bloom_sampler = resources.bloom_sampler },
        trigger = { event = context.frame_sequence_event },
    }):color_target(0, resources.frame_sequence)
end

-- A billboard per light, drawn over the swap chain.
local function add_light_billboards(graph_builder: Builder)
    local resources = graph_builder.resources
    local billboard_params: render_graph.RTParamsRunGraphicsPipelineVK = {
        pipeline = graph_builder.pipelines.light_billboard,
        color_load_op = "load",
        depth_load_op = "load",
        draw_vertex_count = 6,
    }
    for light = 0, graph_builder.context.spot_light_count - 1 do
        graph_builder.light_tasks[light + 1].run_light_billboard = graph_builder.graph:draw(light_id("run_light_billboard", light), billboard_params, {
            bindings = {
                light_billboard_info = resources[light_id("light_billboard_info", light)],
                light_billboard_texture = resources.light_billboard_texture,
                light_billboard_sampler = resources.light_billboard_sampler,
            },
        }):color_target(0, resources.swap_chain):depth_target(resources.gbuffer_depth)
    end
end

-- Shadow and reflective-shadow-map passes of every light.
local function connect_shadow_passes(graph_builder: Builder)
    local resources = graph_builder.resources
    local tasks = graph_builder.tasks
    for light = 0, graph_builder.context.spot_light_count - 1 do
        local light_tasks = graph_builder.light_tasks[light + 1]
        local dynamic_second = light_tasks.dynamic_shadow_second
        local passes = {
            light_tasks.run_shadow_opaque,
            light_tasks.run_shadow_double_sided,
            light_tasks.dynamic_shadow,
            dynamic_second
        }
        local color = resources[light_id("shadowmap_color", light)]
        local normal = resources[light_id("rsm_normal", light)]
        local colors = graph_builder.graph:resource_bundle({ color, normal })
        local moments = resources[light_id("shadow_moments", light)]
        graph_builder.graph:chain({ color, normal, moments, resources[light_id("shadowmap_depth", light)] }, passes)
        graph_builder.graph:chain(colors, { dynamic_second, tasks.run_direct_lighting })
        graph_builder.graph:chain(colors, { dynamic_second, tasks.indirect_lighting })
        -- The radiance injection reads the same reflective-shadow-map slots as indirect lighting.
        if graph_builder.voxel_injects_radiance then
            dynamic_second:connect(tasks.voxel, colors)
        end
        if graph_builder.parameters.summed_area_table then
            local build = light_tasks.build_shadow_sat
            build:inputs_from(dynamic_second, { shadow_moments = moments })
            graph_builder.graph:chain(moments, { dynamic_second, tasks.run_direct_lighting })
            graph_builder.graph:chain(moments, { dynamic_second, tasks.indirect_lighting })
            graph_builder.graph:chain(resources[light_id("shadow_sat", light)], { build, tasks.run_direct_lighting })
            if graph_builder.voxel_injects_radiance then
                dynamic_second:connect(tasks.voxel, moments)
            end
        else
            local mipmaps = light_tasks.generate_shadow_mipmaps
            graph_builder.graph:chain(moments, { dynamic_second, mipmaps, tasks.indirect_lighting })
            graph_builder.graph:chain(resources[light_id("shadow_moments_sampled", light)], { mipmaps, tasks.run_direct_lighting })
            if graph_builder.voxel_injects_radiance then
                mipmaps:connect(tasks.voxel, moments)
            end
        end
    end
end

-- G-buffer passes, and the overlays that depth-test against the G-buffer depth.
local function connect_gbuffer_passes(graph_builder: Builder)
    local resources = graph_builder.resources
    local tasks = graph_builder.tasks
    local gbuffer_passes = {
        tasks.run_gbuffer,
        tasks.run_gbuffer_double_sided,
        tasks.dynamic_gbuffer,
        tasks.dynamic_gbuffer_second,
        tasks.run_direct_lighting
    }
    local gbuffer_resources = graph_builder.graph:resource_bundle({
        resources.gbuffer_albedo,
        resources.gbuffer_normal,
        resources.gbuffer_rma,
        resources.gbuffer_depth
    })
    graph_builder.graph:chain(gbuffer_resources, gbuffer_passes)
    graph_builder.graph:chain(gbuffer_resources, { tasks.dynamic_gbuffer_second, tasks.indirect_lighting })
    graph_builder.graph:chain(resources.gbuffer_rma, { tasks.dynamic_gbuffer_second, tasks.combine_lighting })
    -- The voxel overlays depth-test against the G-buffer depth after lighting has sampled it.
    graph_builder.graph:chain(resources.gbuffer_depth, { tasks.run_direct_lighting, tasks.voxel })
    local depth_overlays = { tasks.indirect_lighting, tasks.voxel }
    for light = 0, graph_builder.context.spot_light_count - 1 do
        depth_overlays[#depth_overlays + 1] = graph_builder.light_tasks[light + 1].run_light_billboard
    end
    graph_builder.graph:chain(resources.gbuffer_depth, depth_overlays)
end

-- Lighting composition, post-processing, bloom, and presentation. output_writer is the last task that writes the
-- result image.
local function connect_composition(graph_builder: Builder, output_writer: render_graph.Task)
    local resources = graph_builder.resources
    local tasks = graph_builder.tasks
    graph_builder.graph:chain(resources.direct_lighting, { tasks.run_direct_lighting, tasks.combine_lighting })
    graph_builder.graph:chain(resources.indirect_lighting, { tasks.indirect_lighting, tasks.combine_lighting })
    output_writer:connect_all({ tasks.extract_bloom, tasks.present_result_image, tasks.present_frame_sequence }, resources.result_image)
    -- The blur task ping-pongs between the bloom images internally; only its final ping result leaves the task.
    graph_builder.graph:chain(resources.bloom_ping, { tasks.extract_bloom, tasks.blur_bloom, tasks.acquire_bloom, tasks.present_result_image })
    -- The frame sequence presents the same result and bloom into its own image, which it then acquires.
    graph_builder.graph:chain(resources.bloom_ping, { tasks.blur_bloom, tasks.present_frame_sequence })
    graph_builder.graph:chain(resources.frame_sequence, { tasks.present_frame_sequence, tasks.acquire_frame_sequence })
    local swap_chain_users = { tasks.present_result_image, tasks.voxel }
    for light = 0, graph_builder.context.spot_light_count - 1 do
        swap_chain_users[#swap_chain_users + 1] = graph_builder.light_tasks[light + 1].run_light_billboard
    end
    swap_chain_users[#swap_chain_users + 1] = tasks.draw_imgui
    graph_builder.graph:chain(resources.swap_chain, swap_chain_users)
end

local exports = {}

function exports.build(context: wbe.DeferredRecipeContext, parameters: wbe.DeferredRecipeParameters): wbe.RendGraphRecipeVK
    local shadow = context.shadow_extent
    local bloom = context.bloom_extent
    local graph = render_graph.new()
    local builder: Builder = {
        context = context,
        parameters = parameters,
        shadow_extent = { width = shadow.width, height = shadow.height },
        bloom_extent = { width = bloom.width, height = bloom.height },
        imports = { "swap_chain", "gbuffer_camera_info", "composition_camera_info", "spot_light_infos", "light_billboard_texture" },
        resources = {},
        static_mesh = graph:register_mesh("static_mesh", context.static_mesh),
        dynamic_mesh = graph:register_mesh("dynamic_mesh", context.dynamic_mesh),
        pipelines = make_pipelines(context),
        graph = graph,
        tasks = {},
        light_tasks = {},
    }
    -- The billboard texture is registered on the graph, which binds the registered view for its import.
    graph:register_image("light_billboard_texture", { texture = "spot_light_billboard" })
    -- The shaders name the acceleration structure and the entities' members by id, so they are declared imports.
    if context.has_mesh_acceleration_structure then
        builder.imports[#builder.imports + 1] = "mesh_acceleration_structure"
    end
    for _, id in ipairs(context.dynamic_mesh_entity_ids) do
        builder.imports[#builder.imports + 1] = id
    end
    for _, id in ipairs(builder.imports) do
        builder.resources[id] = graph:import_resource(id)
    end
    create_light_resources(builder)
    create_screen_resources(builder)
    import_application_tasks(builder)
    add_voxel(builder)
    add_mesh_draws(builder)
    add_shadow_filtering(builder)
    add_lighting(builder)
    local result_writer = add_post_processing(builder)
    add_bloom_and_presentation(builder)
    add_light_billboards(builder)
    connect_shadow_passes(builder)
    connect_gbuffer_passes(builder)
    connect_composition(builder, result_writer)
    return builder.graph:build(builder.imports)
end

return exports
