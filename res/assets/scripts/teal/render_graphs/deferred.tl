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
-- The application imports every pass it builds in C++ as a task (the acceleration structure build, the frame sequence
-- acquisition, and ImGui), and adds its acceleration structure and dynamic mesh entities to the built descriptor. The
-- mesh draws import the meshes the application registered in the render database as "static_mesh" and "dynamic_mesh".
-- It supplies a wbe.DeferredRecipeContext, whose voxel and post_processing entries are the RecipeFeatureParams-derived
-- parameters of the feature modules it selects: a VoxelFeature and a PostProcessingFeature.

local type wbe = require("wbe")
local type voxel = require("render_graphs.features.voxel")
local math_utils = require("utils.math")
local graph_utils = require("utils.render_graph")

local ceil_div = math_utils.ceil_div

-- VK_REMAINING_MIP_LEVELS: a view of every mip level from the base level on.
local REMAINING_MIP_LEVELS = 4294967295
-- One workgroup per 8x8 tile of the lit images, which have the swap chain extent.
local SCREEN_TILE_SIZE = 8
-- One invocation per shadow-map line in the summed-area-table passes.
local SUMMED_AREA_TABLE_WORKGROUP_SIZE = 64
-- One workgroup per 8x8 tile of the bloom images.
local BLOOM_TILE_SIZE = 8

local record VoxelFeature
    -- The structure as a subgraph task, whose overlays draw on the target's color and depth.
    subgraph: function(p_id: string, p_params: any, p_target: voxel.Target): graph_utils.Task
end

local record PostProcessingTarget
    color_view: graph_utils.Resource
    width: integer
    height: integer
end

local record PostProcessingFeature
    -- The effect as a subgraph task, which modifies the target's color view in place.
    subgraph: function(p_id: string, p_params: any, p_target: PostProcessingTarget): graph_utils.Task
end

local function light_id(p_base: string, p_light: integer): string
    return p_base .. "_" .. p_light
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
    run_shadow_opaque: graph_utils.Task
    run_shadow_double_sided: graph_utils.Task
    dynamic_shadow: graph_utils.Task
    dynamic_shadow_second: graph_utils.Task
    build_shadow_sat: graph_utils.Task
    generate_shadow_mipmaps: graph_utils.Task
    run_light_billboard: graph_utils.Task
end

local record Tasks
    build_mesh_acceleration_structure: graph_utils.Task
    acquire_frame_sequence: graph_utils.Task
    draw_imgui: graph_utils.Task
    voxel: graph_utils.Task
    run_gbuffer: graph_utils.Task
    run_gbuffer_double_sided: graph_utils.Task
    dynamic_gbuffer: graph_utils.Task
    dynamic_gbuffer_second: graph_utils.Task
    run_direct_lighting: graph_utils.Task
    indirect_lighting: {graph_utils.Task}
    combine_lighting: graph_utils.Task
    post_processing: graph_utils.Task
    extract_bloom: graph_utils.Task
    blur_bloom: graph_utils.Task
    acquire_bloom: graph_utils.Task
    present_result_image: graph_utils.Task
    present_frame_sequence: graph_utils.Task
end

local record Builder
    context: wbe.DeferredRecipeContext
    shadow_extent: wbe.RendExtent2DParamsVK
    bloom_extent: wbe.RendExtent2DParamsVK
    imports: {string}
    resources: {string:graph_utils.Resource}
    graph: graph_utils.Graph
    tasks: Tasks
    light_tasks: {LightTasks}
end

local record MeshTargets
    colors: {graph_utils.Resource}
    depth: graph_utils.Resource
end

local function import_task(p_builder: Builder, p_id: string): graph_utils.Task
    return p_builder.graph:add({ id = p_id, import = true })
end

-- Indirect lighting runs as two toggled tasks, the normal one and the voxel debug view. Expand its first handle to both.
local function chain(p_builder: Builder, p_resource: graph_utils.Resource, p_tasks: {graph_utils.Task})
    local expanded: {graph_utils.Task} = {}
    for _, task in ipairs(p_tasks) do
        if task == p_builder.tasks.indirect_lighting[1] then
            for _, indirect in ipairs(p_builder.tasks.indirect_lighting) do
                expanded[#expanded + 1] = indirect
            end
        else
            expanded[#expanded + 1] = task
        end
    end
    p_builder.graph:chain(p_resource, expanded)
end

local function compute(p_builder: Builder, p_id: string, p_pipeline: string, p_group_count_x: integer,
    p_group_count_y: integer, p_aliases?: {string:graph_utils.Resource}): graph_utils.Task
    local params: wbe.RTParamsRunComputePipelineVK =
        { pipeline = p_pipeline, group_count_x = p_group_count_x, group_count_y = p_group_count_y, queue_family = "compute_1" }
    local task = p_builder.graph:add({ id = p_id, type = "run_compute_pipeline", params = params })
    for name, resource in pairs(p_aliases or {}) do
        task:alias(name, resource)
    end
    return task
end

local function draw_mesh(p_builder: Builder, p_id: string, p_mesh: string, p_pipeline: string, p_part: wbe.MeshPartVK,
    p_load: boolean, p_extent: wbe.RendExtent2DParamsVK, p_clear_values: {wbe.RTParamsClearValueVK},
    p_targets: MeshTargets, p_aliases?: {string:graph_utils.Resource}, p_transform?: graph_utils.Resource): graph_utils.Task
    local load_op: wbe.AttachmentLoadOpVK = p_load and "load" or "clear"
    local params: wbe.RTParamsGraphicsDrawMeshVK = {
        mesh = p_mesh,
        pipeline = p_pipeline,
        parts = { p_part },
        transform = p_transform and p_transform.id,
        render_extent = p_extent,
        clear_values = p_clear_values,
        color_load_op = load_op,
        depth_load_op = load_op,
    }
    local task = p_builder.graph:add({ id = p_id, type = "graphics_draw_mesh", params = params,
        uses = p_transform and { p_transform } or {} })
    task:alias("render_target_depth", p_targets.depth)
    for index, color in ipairs(p_targets.colors) do
        task:alias("render_target_colors[" .. (index - 1) .. "]", color)
    end
    for name, resource in pairs(p_aliases or {}) do
        task:alias(name, resource)
    end
    return task
end

-- Shadow and reflective-shadow-map targets of every light. Every light shares one shadow sampler.
local function create_light_resources(p_builder: Builder)
    local context = p_builder.context
    local formats = context.formats
    local shadow_extent = p_builder.shadow_extent
    local resources = p_builder.resources
    for light = 0, context.spot_light_count - 1 do
        local light_info = light_id("shadow_light_info", light)
        local billboard_info = light_id("light_billboard_info", light)
        p_builder.imports[#p_builder.imports + 1] = light_info
        p_builder.imports[#p_builder.imports + 1] = billboard_info
        resources[light_info] = graph_utils.import_resource(light_info)
        resources[billboard_info] = graph_utils.import_resource(billboard_info)
        local depth = graph_utils.image(light_id("shadowmap_depth_image", light), "image_depth_target", formats.shadowmap_depth,
            shadow_extent)
        local color = graph_utils.image(light_id("shadowmap_color_image", light), "image_render_target", formats.rsm_flux,
            shadow_extent)
        local normal = graph_utils.image(light_id("rsm_normal_image", light), "image_render_target", formats.rsm_normal,
            shadow_extent)
        local moments = graph_utils.image(light_id("shadow_moments_image", light), "image_render_target", formats.shadow_moments,
            shadow_extent, { "transfer_src", "transfer_dst" }, context.shadow_mipmap_level_count)
        local horizontal = graph_utils.image(light_id("shadow_moments_sat_horizontal_image", light), "image_read_write",
            formats.shadow_moments, shadow_extent)
        local summed = graph_utils.image(light_id("shadow_moments_sat_image", light), "image_read_write", formats.shadow_moments,
            shadow_extent)
        resources[light_id("shadowmap_depth", light)] = graph_utils.image_view(light_id("shadowmap_depth", light), depth)
        resources[light_id("shadowmap_color", light)] = graph_utils.image_view(light_id("shadowmap_color", light), color)
        resources[light_id("rsm_normal", light)] = graph_utils.image_view(light_id("rsm_normal", light), normal)
        resources[light_id("shadow_moments", light)] = graph_utils.image_view(light_id("shadow_moments", light), moments)
        -- Direct lighting samples the complete mip chain through a second view of the moments image.
        resources[light_id("shadow_moments_sampled", light)] = graph_utils.image_view(light_id("shadow_moments_sampled", light),
            moments, { mip_level_count = REMAINING_MIP_LEVELS })
        resources[light_id("shadow_sat_horizontal", light)] = graph_utils.image_view(light_id("shadow_sat_horizontal", light), horizontal)
        resources[light_id("shadow_sat", light)] = graph_utils.image_view(light_id("shadow_sat", light), summed)
    end
    resources.shadow_sampler = graph_utils.clamp_to_edge_sampler("shadow_sampler",
        { min_lod = 0.0, max_lod = context.shadow_mipmap_level_count - 1.0 })
end

-- The G-buffer targets and the lighting and post-processing outputs. Views retain their source image handles.
local function create_screen_resources(p_builder: Builder)
    local context = p_builder.context
    local formats = context.formats
    local bloom_extent = p_builder.bloom_extent
    local resources = p_builder.resources
    resources.gbuffer_albedo = graph_utils.image_view("gbuffer_albedo",
        graph_utils.image("gbuffer_albedo_image", "image_render_target", formats.gbuffer_albedo))
    resources.gbuffer_normal = graph_utils.image_view("gbuffer_normal",
        graph_utils.image("gbuffer_normal_image", "image_render_target", formats.gbuffer_normal))
    resources.gbuffer_rma = graph_utils.image_view("gbuffer_rma",
        graph_utils.image("gbuffer_rma_image", "image_render_target", formats.gbuffer_rma))
    resources.gbuffer_depth = graph_utils.image_view("gbuffer_depth",
        graph_utils.image("gbuffer_depth_image", "image_depth_target", formats.gbuffer_depth))
    resources.gbuffer_sampler = graph_utils.clamp_to_edge_sampler("gbuffer_sampler", { min_filter = "nearest", mag_filter = "nearest" })

    -- The bloom images keep the extent of the initial swap chain, because the bloom readback buffer is sized by it.
    resources.direct_lighting = graph_utils.image_view("direct_lighting",
        graph_utils.image("direct_lighting_image", "image_read_write", formats.lighting_output))
    resources.indirect_lighting = graph_utils.image_view("indirect_lighting",
        graph_utils.image("indirect_lighting_image", "image_read_write", formats.lighting_output))
    resources.result_image = graph_utils.image_view("result_image",
        graph_utils.image("result_color_image", "image_read_write", formats.lighting_output))
    resources.bloom_ping = graph_utils.image_view("bloom_ping",
        graph_utils.image("bloom_ping_image", "image_read_write", formats.lighting_output, bloom_extent, { "transfer_src" }))
    resources.bloom_pong = graph_utils.image_view("bloom_pong",
        graph_utils.image("bloom_pong_image", "image_read_write", formats.lighting_output, bloom_extent))
    -- The present pipeline renders in the swap-chain format, so the sequence matches the presented image.
    resources.frame_sequence = graph_utils.image_view("frame_sequence",
        graph_utils.image("frame_sequence_image", "image_render_target", context.swap_chain_format))
    resources.bloom_sampler = graph_utils.clamp_to_edge_sampler("bloom_sampler")
    resources.light_billboard_sampler = graph_utils.clamp_to_edge_sampler("light_billboard_sampler")
end

-- Tasks the application builds in C++.
local function import_application_tasks(p_builder: Builder)
    if p_builder.context.has_mesh_acceleration_structure then
        p_builder.tasks.build_mesh_acceleration_structure = import_task(p_builder, "build_mesh_acceleration_structure")
    end
    p_builder.tasks.acquire_frame_sequence = import_task(p_builder, "acquire_frame_sequence")
    p_builder.tasks.draw_imgui = import_task(p_builder, "draw_imgui")
end

-- The voxel structure, a subgraph whose overlays draw over the lit scene.
local function add_voxel(p_builder: Builder)
    local resources = p_builder.resources
    local context = p_builder.context
    local voxel_params = context.voxel as wbe.RecipeFeatureParams
    local voxel_feature = require(voxel_params.module) as VoxelFeature
    p_builder.tasks.voxel = p_builder.graph:add(voxel_feature.subgraph(VOXEL, voxel_params, {
        dynamic_mesh_transform_ids = context.dynamic_mesh_transform_ids,
        debug_view_toggles = context.voxel_debug_view_toggles,
        color_target = resources.swap_chain,
        depth_target = resources.gbuffer_depth,
    }))
end

-- Mesh draws. The double-sided and dynamic passes load what the previous pass of their targets wrote.
local function add_mesh_draws(p_builder: Builder)
    local resources = p_builder.resources
    local dynamic_transforms = p_builder.context.dynamic_mesh_transform_ids
    if #dynamic_transforms ~= 2 then
        error("deferred: the dynamic mesh draws two entities, but " .. #dynamic_transforms .. " transforms are supplied.")
    end
    local shadow_extent = p_builder.shadow_extent
    for light = 0, p_builder.context.spot_light_count - 1 do
        local tasks: LightTasks = {}
        p_builder.light_tasks[light + 1] = tasks
        local targets: MeshTargets = {
            colors = { resources[light_id("shadowmap_color", light)], resources[light_id("rsm_normal", light)],
                resources[light_id("shadow_moments", light)] },
            depth = resources[light_id("shadowmap_depth", light)],
        }
        local clear_values = { CLEAR_BLACK, CLEAR_BLACK, CLEAR_MOMENTS, CLEAR_DEPTH }
        local light_aliases: {string:graph_utils.Resource} = { shadow_light_info = resources[light_id("shadow_light_info", light)] }
        tasks.run_shadow_opaque = draw_mesh(p_builder, light_id("run_shadow_opaque", light), "static_mesh", "deferred_shadow_opaque", "opaque", false,
            shadow_extent, clear_values, targets, light_aliases)
        tasks.run_shadow_double_sided = draw_mesh(p_builder, light_id("run_shadow_double_sided", light), "static_mesh", "deferred_shadow_double_sided",
            "double_sided", true, shadow_extent, clear_values, targets, light_aliases)
        tasks.dynamic_shadow = draw_mesh(p_builder, light_id("dynamic_shadow", light), "dynamic_mesh", "deferred_shadow_dynamic", "opaque", true,
            shadow_extent, {}, targets, light_aliases, graph_utils.reference(dynamic_transforms[1]))
        tasks.dynamic_shadow_second = draw_mesh(p_builder, light_id("dynamic_shadow_second", light), "dynamic_mesh", "deferred_shadow_dynamic", "opaque", true,
            shadow_extent, {}, targets, light_aliases, graph_utils.reference(dynamic_transforms[2]))
    end
    local gbuffer_targets: MeshTargets = { colors = { resources.gbuffer_albedo, resources.gbuffer_normal, resources.gbuffer_rma }, depth = resources.gbuffer_depth }
    local gbuffer_clear_values = { CLEAR_BLACK, CLEAR_BLACK, CLEAR_BLACK, CLEAR_DEPTH }
    p_builder.tasks.run_gbuffer = draw_mesh(p_builder, "run_gbuffer", "static_mesh", "deferred_gbuffer_opaque", "opaque", false, nil, gbuffer_clear_values,
        gbuffer_targets)
    p_builder.tasks.run_gbuffer_double_sided = draw_mesh(p_builder, "run_gbuffer_double_sided", "static_mesh", "deferred_gbuffer_double_sided", "double_sided", true, nil,
        gbuffer_clear_values, gbuffer_targets)
    p_builder.tasks.dynamic_gbuffer = draw_mesh(p_builder, "dynamic_gbuffer", "dynamic_mesh", "deferred_gbuffer_dynamic", "opaque", true, nil, {}, gbuffer_targets,
        nil, graph_utils.reference(dynamic_transforms[1]))
    p_builder.tasks.dynamic_gbuffer_second = draw_mesh(p_builder, "dynamic_gbuffer_second", "dynamic_mesh", "deferred_gbuffer_dynamic", "opaque", true, nil, {},
        gbuffer_targets, nil, graph_utils.reference(dynamic_transforms[2]))
end

-- Shadow filtering of every light.
local function add_shadow_filtering(p_builder: Builder)
    local resources = p_builder.resources
    local context = p_builder.context
    local shadow = context.shadow_extent
    for light = 0, context.spot_light_count - 1 do
        local tasks = p_builder.light_tasks[light + 1]
        if context.summed_area_table then
            local params: wbe.RTParamsRunComputePipelinesSequentialVK = {
                queue_family = "compute_1",
                dispatches = {
                    { pipeline = "shadow_moments_summed_area_table_horizontal_shader", group_count_x = ceil_div(shadow.height, SUMMED_AREA_TABLE_WORKGROUP_SIZE) },
                    { pipeline = "shadow_moments_summed_area_table_vertical_shader", group_count_x = ceil_div(shadow.width, SUMMED_AREA_TABLE_WORKGROUP_SIZE) },
                },
            }
            tasks.build_shadow_sat = p_builder.graph:add({
                id = light_id("build_shadow_sat", light),
                type = "run_compute_pipelines_sequential",
                params = params,
                aliases = {
                    shadow_moments = resources[light_id("shadow_moments", light)],
                    shadow_sat_horizontal = resources[light_id("shadow_sat_horizontal", light)],
                    shadow_sat = resources[light_id("shadow_sat", light)],
                },
            })
        else
            local params: wbe.RTParamsGenerateMipmapsVK = {
                source_view = resources[light_id("shadow_moments", light)].id,
                mip_chain_view = resources[light_id("shadow_moments_sampled", light)].id,
                base_extent = p_builder.shadow_extent,
                mipmap_level_count = context.shadow_mipmap_level_count,
                format = context.formats.shadow_moments,
            }
            tasks.generate_shadow_mipmaps = p_builder.graph:add({ id = light_id("generate_shadow_mipmaps", light), type = "generate_mipmaps", params = params,
                uses = { resources[light_id("shadow_moments", light)], resources[light_id("shadow_moments_sampled", light)] } })
        end
    end
end

-- Lighting. Every slot of the lighting shaders' lights[] array maps to a light's shadow resources; slots past the light
-- count reuse light zero, so that every reflected descriptor binds.
local function add_lighting(p_builder: Builder)
    local resources = p_builder.resources
    local context = p_builder.context
    local summed_area_table = context.summed_area_table
    local light_count = context.spot_light_count
    local screen = context.swap_chain_extent
    local light_slots: {string:string}
    if summed_area_table then
        light_slots = { shadow_moments = "shadow_moments", shadow_sat = "shadow_sat", shadowmap_color = "shadowmap_color", rsm_normal = "rsm_normal" }
    else
        light_slots = { shadow_moments_sampled = "shadow_moments_sampled", shadowmap_color = "shadowmap_color", rsm_normal = "rsm_normal" }
    end
    local direct_lighting_aliases: {string:graph_utils.Resource} = {
        gbuffer_sampler = resources.gbuffer_sampler,
        shadow_sampler = resources.shadow_sampler,
    }
    for slot = 0, context.max_spot_light_info_count - 1 do
        local light = slot < light_count and slot or 0
        for task_resource, graph_resource in pairs(light_slots) do
            direct_lighting_aliases[task_resource .. "[" .. slot .. "]"] = resources[light_id(graph_resource, light)]
        end
    end
    local screen_tiles_x = ceil_div(screen.width, SCREEN_TILE_SIZE)
    local screen_tiles_y = ceil_div(screen.height, SCREEN_TILE_SIZE)
    p_builder.tasks.run_direct_lighting = compute(p_builder, "run_direct_lighting",
        summed_area_table and "direct_lighting_summed_area_table_shader" or "direct_lighting_shader", screen_tiles_x,
        screen_tiles_y, direct_lighting_aliases)
    -- Indirect lighting only loads mip level zero of the moments, so its mip-chain slot binds the base moments view, which
    -- the summed-area-table graph orders without a mip generation pass. The debug view variant replaces it while a voxel
    -- debug view is shown.
    local indirect_lighting_aliases: {string:graph_utils.Resource} = {
        gbuffer_sampler = resources.gbuffer_sampler,
        shadow_sampler = resources.shadow_sampler,
    }
    for slot = 0, context.max_spot_light_info_count - 1 do
        local light = slot < light_count and slot or 0
        indirect_lighting_aliases["shadow_moments_sampled[" .. slot .. "]"] = resources[light_id("shadow_moments", light)]
        indirect_lighting_aliases["shadowmap_color[" .. slot .. "]"] = resources[light_id("shadowmap_color", light)]
        indirect_lighting_aliases["rsm_normal[" .. slot .. "]"] = resources[light_id("rsm_normal", light)]
    end
    local debug_view_toggles = context.voxel_debug_view_toggles
    for index, pipeline in ipairs({ "indirect_lighting_rsm_shader", "indirect_lighting_rsm_debug_view_shader" }) do
        local params: wbe.RTParamsRunComputePipelineVK =
            { pipeline = pipeline, group_count_x = screen_tiles_x, group_count_y = screen_tiles_y, queue_family = "compute_2" }
        p_builder.tasks.indirect_lighting[index] = p_builder.graph:add({
            id = index == 1 and "run_indirect_lighting" or "run_indirect_lighting_debug_view",
            type = "run_compute_pipeline",
            params = params,
            aliases = indirect_lighting_aliases,
            toggle = index == 1 and debug_view_toggles.disabled or debug_view_toggles.enabled,
        })
    end
    p_builder.tasks.combine_lighting = compute(p_builder, "combine_lighting", "combine_lighting_shader", screen_tiles_x, screen_tiles_y)
end

-- Post-processing, a subgraph, modifies the result image in place after composition. Returns the last task that writes
-- the result image.
local function add_post_processing(p_builder: Builder): graph_utils.Task
    local resources = p_builder.resources
    local post_processing = p_builder.context.post_processing as wbe.RecipeFeatureParams
    if post_processing == nil then
        return p_builder.tasks.combine_lighting
    end
    local screen = p_builder.context.swap_chain_extent
    local post_processing_feature = require(post_processing.module) as PostProcessingFeature
    p_builder.tasks.post_processing = p_builder.graph:add(post_processing_feature.subgraph(POST_PROCESSING, post_processing,
        { color_view = resources.result_image, width = screen.width, height = screen.height }))
    chain(p_builder, resources.result_image, { p_builder.tasks.combine_lighting, p_builder.tasks.post_processing })
    return p_builder.tasks.post_processing
end

-- Bloom, presentation, and on-demand acquisition.
local function add_bloom_and_presentation(p_builder: Builder)
    local resources = p_builder.resources
    local context = p_builder.context
    local bloom = context.bloom_extent
    local graph = p_builder.graph
    local bloom_tiles_x = ceil_div(bloom.width, BLOOM_TILE_SIZE)
    local bloom_tiles_y = ceil_div(bloom.height, BLOOM_TILE_SIZE)
    p_builder.tasks.extract_bloom = compute(p_builder, "extract_bloom", "bloom_shader_extract", bloom_tiles_x, bloom_tiles_y,
        { bloom_sampler = resources.bloom_sampler })
    -- Starting from ping, even iterations blur ping into pong and odd iterations blur pong back into ping, so an even
    -- iteration count leaves the result in ping.
    local blur_params: wbe.RTParamsPingPongVK = {
        ping_pipeline = "kawase_blur_pong_write_shader",
        pong_pipeline = "kawase_blur_ping_write_shader",
        iterations = context.bloom_blur_pass_count,
        group_count_x = bloom_tiles_x,
        group_count_y = bloom_tiles_y,
        queue_family = "compute_1",
    }
    p_builder.tasks.blur_bloom = graph:add({
        id = "blur_bloom",
        type = "ping_pong",
        params = blur_params,
        aliases = { kawase_blur_ping = resources.bloom_ping, kawase_blur_pong = resources.bloom_pong, kawase_blur_sampler = resources.bloom_sampler },
    })
    local acquire_params: wbe.RTParamsCPUInFlightAcquireVK = { image_view = resources.bloom_ping.id }
    p_builder.tasks.acquire_bloom = graph:add({
        id = "acquire_bloom",
        type = "cpu_acquire_image",
        params = acquire_params,
        uses = { resources.bloom_ping },
        trigger = { event = context.bloom_acquire_event },
    })
    local present_params: wbe.RTParamsRunGraphicsPipelineVK = { pipeline = "present_post_processing", clear_values = { { color = { 0.0, 0.0, 0.0, 1.0 } } } }
    p_builder.tasks.present_result_image = graph:add({
        id = "present_result_image",
        type = "run_graphics_pipeline",
        params = present_params,
        aliases = { ["render_target_colors[0]"] = resources.swap_chain, bloom_sampler = resources.bloom_sampler },
    })
    p_builder.tasks.present_frame_sequence = graph:add({
        id = "present_frame_sequence",
        type = "run_graphics_pipeline",
        params = present_params,
        aliases = { ["render_target_colors[0]"] = resources.frame_sequence, bloom_sampler = resources.bloom_sampler },
        trigger = { event = context.frame_sequence_event },
    })
end

-- A billboard per light, drawn over the swap chain.
local function add_light_billboards(p_builder: Builder)
    local resources = p_builder.resources
    local billboard_params: wbe.RTParamsRunGraphicsPipelineVK = {
        pipeline = "deferred_light_billboard_pipeline",
        color_load_op = "load",
        depth_load_op = "load",
        draw_vertex_count = 6,
    }
    for light = 0, p_builder.context.spot_light_count - 1 do
        p_builder.light_tasks[light + 1].run_light_billboard = p_builder.graph:add({
            id = light_id("run_light_billboard", light),
            type = "run_graphics_pipeline",
            params = billboard_params,
            aliases = {
                light_billboard_info = resources[light_id("light_billboard_info", light)],
                light_billboard_texture = resources.light_billboard_texture,
                light_billboard_sampler = resources.light_billboard_sampler,
                ["render_target_colors[0]"] = resources.swap_chain,
                render_target_depth = resources.gbuffer_depth,
            },
        })
    end
end

-- Shadow and reflective-shadow-map passes of every light.
local function connect_shadow_passes(p_builder: Builder)
    local resources = p_builder.resources
    local tasks = p_builder.tasks
    for light = 0, p_builder.context.spot_light_count - 1 do
        local light_tasks = p_builder.light_tasks[light + 1]
        local dynamic_second = light_tasks.dynamic_shadow_second
        local passes = { light_tasks.run_shadow_opaque, light_tasks.run_shadow_double_sided, light_tasks.dynamic_shadow, dynamic_second }
        for _, target in ipairs({ "shadowmap_color", "rsm_normal", "shadow_moments", "shadowmap_depth" }) do
            chain(p_builder, resources[light_id(target, light)], passes)
        end
        for _, color in ipairs({ "shadowmap_color", "rsm_normal" }) do
            chain(p_builder, resources[light_id(color, light)], { dynamic_second, tasks.run_direct_lighting })
            chain(p_builder, resources[light_id(color, light)], { dynamic_second, tasks.indirect_lighting[1] })
        end
        local moments = resources[light_id("shadow_moments", light)]
        if p_builder.context.summed_area_table then
            local build = light_tasks.build_shadow_sat
            chain(p_builder, moments, { dynamic_second, build })
            chain(p_builder, moments, { dynamic_second, tasks.run_direct_lighting })
            chain(p_builder, moments, { dynamic_second, tasks.indirect_lighting[1] })
            chain(p_builder, resources[light_id("shadow_sat", light)], { build, tasks.run_direct_lighting })
        else
            local mipmaps = light_tasks.generate_shadow_mipmaps
            chain(p_builder, moments, { dynamic_second, mipmaps, tasks.indirect_lighting[1] })
            chain(p_builder, resources[light_id("shadow_moments_sampled", light)], { mipmaps, tasks.run_direct_lighting })
        end
    end
end

-- G-buffer passes, and the overlays that depth-test against the G-buffer depth.
local function connect_gbuffer_passes(p_builder: Builder)
    local resources = p_builder.resources
    local tasks = p_builder.tasks
    local gbuffer_passes = { tasks.run_gbuffer, tasks.run_gbuffer_double_sided, tasks.dynamic_gbuffer, tasks.dynamic_gbuffer_second, tasks.run_direct_lighting }
    for _, color in ipairs({ resources.gbuffer_albedo, resources.gbuffer_normal, resources.gbuffer_rma }) do
        chain(p_builder, color, gbuffer_passes)
        chain(p_builder, color, { tasks.dynamic_gbuffer_second, tasks.indirect_lighting[1] })
    end
    chain(p_builder, resources.gbuffer_rma, { tasks.dynamic_gbuffer_second, tasks.combine_lighting })
    chain(p_builder, resources.gbuffer_depth, gbuffer_passes)
    chain(p_builder, resources.gbuffer_depth, { tasks.dynamic_gbuffer_second, tasks.indirect_lighting[1] })
    -- The voxel overlays depth-test against the G-buffer depth after lighting has sampled it.
    chain(p_builder, resources.gbuffer_depth, { tasks.run_direct_lighting, tasks.voxel })
    local depth_overlays = { tasks.indirect_lighting[1], tasks.voxel }
    for light = 0, p_builder.context.spot_light_count - 1 do
        depth_overlays[#depth_overlays + 1] = p_builder.light_tasks[light + 1].run_light_billboard
    end
    chain(p_builder, resources.gbuffer_depth, depth_overlays)
end

-- Lighting composition, post-processing, bloom, and presentation. p_result_writer is the last task that writes the
-- result image.
local function connect_composition(p_builder: Builder, p_result_writer: graph_utils.Task)
    local resources = p_builder.resources
    local tasks = p_builder.tasks
    chain(p_builder, resources.direct_lighting, { tasks.run_direct_lighting, tasks.combine_lighting })
    chain(p_builder, resources.indirect_lighting, { tasks.indirect_lighting[1], tasks.combine_lighting })
    chain(p_builder, resources.result_image, { p_result_writer, tasks.extract_bloom })
    chain(p_builder, resources.result_image, { p_result_writer, tasks.present_result_image })
    -- The blur task ping-pongs between the bloom images internally; only its final ping result leaves the task.
    chain(p_builder, resources.bloom_ping, { tasks.extract_bloom, tasks.blur_bloom, tasks.acquire_bloom, tasks.present_result_image })
    -- The frame sequence presents the same result and bloom into its own image, which it then acquires.
    chain(p_builder, resources.result_image, { p_result_writer, tasks.present_frame_sequence })
    chain(p_builder, resources.bloom_ping, { tasks.blur_bloom, tasks.present_frame_sequence })
    chain(p_builder, resources.frame_sequence, { tasks.present_frame_sequence, tasks.acquire_frame_sequence })
    local swap_chain_users = { tasks.present_result_image, tasks.voxel }
    for light = 0, p_builder.context.spot_light_count - 1 do
        swap_chain_users[#swap_chain_users + 1] = p_builder.light_tasks[light + 1].run_light_billboard
    end
    swap_chain_users[#swap_chain_users + 1] = tasks.draw_imgui
    chain(p_builder, resources.swap_chain, swap_chain_users)
end

local exports = {}

function exports.build(context: wbe.DeferredRecipeContext): wbe.RendGraphRecipeVK
    local shadow = context.shadow_extent
    local bloom = context.bloom_extent
    local builder: Builder = {
        context = context,
        shadow_extent = { width = shadow.width, height = shadow.height },
        bloom_extent = { width = bloom.width, height = bloom.height },
        imports = { "swap_chain", "gbuffer_camera_info", "composition_camera_info", "spot_light_infos", "light_billboard_texture" },
        resources = {},
        graph = graph_utils.graph(),
        tasks = { indirect_lighting = {} },
        light_tasks = {},
    }
    for _, id in ipairs(builder.imports) do
        builder.resources[id] = graph_utils.import_resource(id)
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
