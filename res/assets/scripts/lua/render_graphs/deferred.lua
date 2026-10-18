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
-- acquisition, and ImGui), and adds its acceleration structure and dynamic mesh entities to the built descriptor. The mesh draws import the meshes the application registered in the render
-- database as "static_mesh" and "dynamic_mesh". Context entries it supplies, besides the renderer facts:
--   summed_area_table: whether shadows are filtered with summed-area tables instead of mipmaps.
--   spot_light_count, max_spot_light_info_count: lights in the scene, and slots of the lighting shaders' lights[].
--   shadow_extent, bloom_extent: { width, height } of every light's shadow targets and of the bloom images.
--   shadow_mipmap_level_count, bloom_blur_pass_count.
--   formats: the image formats of the targets, by role.
--   bloom_acquire_event, frame_sequence_event: the trigger event ids of this graph.
--   has_mesh_acceleration_structure: whether the acceleration structure build task is imported.
--   voxel: { module = <module name>, ... }, the voxel structure, passed to that module's add(), which returns its
--   overlays in drawing order.
--   dynamic_mesh_transform_ids: the graph ids of the dynamic mesh entities' transforms, one per drawn entity.
--   voxel_debug_view_toggles: { disabled, enabled, post_svo_build, pre_deferred_propagation }, the RTToggleVK toggles
--   ({ event, enabled }) of the tasks each voxel debug view mode group records.
--   post_processing: nil, or { module = <module name>, ... } passed to that module's add().

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

local CLAMP_TO_EDGE = { address_mode_u = "clamp_to_edge", address_mode_v = "clamp_to_edge", address_mode_w = "clamp_to_edge" }

local function image(p_id, p_usage, p_format, p_extent, p_extra)
    local params = { usage = p_usage, format = p_format, extent = p_extent }
    for key, value in pairs(p_extra or {}) do
        params[key] = value
    end
    return { id = p_id, image = params }
end

local function sampler(p_id, p_params)
    local params = {}
    for key, value in pairs(CLAMP_TO_EDGE) do
        params[key] = value
    end
    for key, value in pairs(p_params or {}) do
        params[key] = value
    end
    return { id = p_id, sampler = params }
end

local function light_id(p_base, p_light)
    return p_base .. "_" .. p_light
end

local exports = {}

---@param context wbe.RecipeContext
---@return wbe.RendGraphRecipeVK
function exports.build(context)
    local summed_area_table = context.summed_area_table
    local light_count = context.spot_light_count
    local formats = context.formats
    local screen = context.swap_chain_extent
    local shadow = context.shadow_extent
    local bloom = context.bloom_extent
    local shadow_extent = { width = shadow.width, height = shadow.height }
    local bloom_extent = { width = bloom.width, height = bloom.height }
    local imports = { "swap_chain", "gbuffer_camera_info", "composition_camera_info", "spot_light_infos", "light_billboard_texture" }
    local resources = {}
    local tasks = graph_utils.task_list()
    -- Indirect lighting runs as two toggled tasks, the normal one and the voxel debug view, so every chain through it
    -- passes both, in that order.
    local INDIRECT_LIGHTING_TASKS = { "run_indirect_lighting", "run_indirect_lighting_debug_view" }
    local function chain(p_resource, p_task_ids)
        local expanded = {}
        for _, id in ipairs(p_task_ids) do
            if id == "run_indirect_lighting" then
                for _, indirect in ipairs(INDIRECT_LIGHTING_TASKS) do
                    expanded[#expanded + 1] = indirect
                end
            else
                expanded[#expanded + 1] = id
            end
        end
        tasks.chain(p_resource, expanded)
    end
    local function add_resource(p_resource)
        resources[#resources + 1] = p_resource
    end
    local function import_task(p_id)
        tasks.add({ id = p_id, import = true })
    end
    local function compute(p_id, p_pipeline, p_group_count_x, p_group_count_y, p_aliases)
        tasks.add({
            id = p_id,
            type = "run_compute_pipeline",
            params = { pipeline = p_pipeline, group_count_x = p_group_count_x, group_count_y = p_group_count_y, queue_family = "compute_1" },
            aliases = p_aliases,
        })
    end

    -- Shadow and reflective-shadow-map targets of every light. Every light shares one shadow sampler.
    for light = 0, light_count - 1 do
        imports[#imports + 1] = light_id("shadow_light_info", light)
        imports[#imports + 1] = light_id("light_billboard_info", light)
        add_resource(image(light_id("shadowmap_depth_image", light), "image_depth_target", formats.shadowmap_depth, shadow_extent))
        add_resource(image(light_id("shadowmap_color_image", light), "image_render_target", formats.rsm_flux, shadow_extent))
        add_resource(image(light_id("rsm_normal_image", light), "image_render_target", formats.rsm_normal, shadow_extent))
        add_resource(image(light_id("shadow_moments_image", light), "image_render_target", formats.shadow_moments, shadow_extent,
            { additional_usage = { "transfer_src", "transfer_dst" }, mip_levels = context.shadow_mipmap_level_count }))
        add_resource(image(light_id("shadow_moments_sat_horizontal_image", light), "image_read_write", formats.shadow_moments, shadow_extent))
        add_resource(image(light_id("shadow_moments_sat_image", light), "image_read_write", formats.shadow_moments, shadow_extent))
        add_resource(graph_utils.image_view(light_id("shadowmap_depth", light), light_id("shadowmap_depth_image", light)))
        add_resource(graph_utils.image_view(light_id("shadowmap_color", light), light_id("shadowmap_color_image", light)))
        add_resource(graph_utils.image_view(light_id("rsm_normal", light), light_id("rsm_normal_image", light)))
        add_resource(graph_utils.image_view(light_id("shadow_moments", light), light_id("shadow_moments_image", light)))
        -- Direct lighting samples the complete mip chain through a second view of the moments image.
        add_resource({
            id = light_id("shadow_moments_sampled", light),
            image_view = { image = light_id("shadow_moments_image", light), view = { mip_level_count = REMAINING_MIP_LEVELS } },
        })
        add_resource(graph_utils.image_view(light_id("shadow_sat_horizontal", light), light_id("shadow_moments_sat_horizontal_image", light)))
        add_resource(graph_utils.image_view(light_id("shadow_sat", light), light_id("shadow_moments_sat_image", light)))
    end
    add_resource(sampler("shadow_sampler", { min_lod = 0.0, max_lod = context.shadow_mipmap_level_count - 1.0 }))

    -- G-buffer targets.
    add_resource(image("gbuffer_albedo_image", "image_render_target", formats.gbuffer_albedo))
    add_resource(image("gbuffer_normal_image", "image_render_target", formats.gbuffer_normal))
    add_resource(image("gbuffer_rma_image", "image_render_target", formats.gbuffer_rma))
    add_resource(image("gbuffer_depth_image", "image_depth_target", formats.gbuffer_depth))
    add_resource(graph_utils.image_view("gbuffer_albedo", "gbuffer_albedo_image"))
    add_resource(graph_utils.image_view("gbuffer_normal", "gbuffer_normal_image"))
    add_resource(graph_utils.image_view("gbuffer_rma", "gbuffer_rma_image"))
    add_resource(graph_utils.image_view("gbuffer_depth", "gbuffer_depth_image"))
    add_resource(sampler("gbuffer_sampler", { min_filter = "nearest", mag_filter = "nearest" }))

    -- Lighting and post-processing outputs. The bloom images keep the extent of the initial swap chain, because the bloom
    -- readback buffer is sized by it.
    add_resource(image("direct_lighting_image", "image_read_write", formats.lighting_output))
    add_resource(image("indirect_lighting_image", "image_read_write", formats.lighting_output))
    add_resource(image("result_color_image", "image_read_write", formats.lighting_output))
    add_resource(image("bloom_ping_image", "image_read_write", formats.lighting_output, bloom_extent, { additional_usage = { "transfer_src" } }))
    add_resource(image("bloom_pong_image", "image_read_write", formats.lighting_output, bloom_extent))
    -- The present pipeline renders in the swap-chain format, so the sequence matches the presented image.
    add_resource(image("frame_sequence_image", "image_render_target", context.swap_chain_format))
    add_resource(graph_utils.image_view("direct_lighting", "direct_lighting_image"))
    add_resource(graph_utils.image_view("indirect_lighting", "indirect_lighting_image"))
    add_resource(graph_utils.image_view("result_image", "result_color_image"))
    add_resource(graph_utils.image_view("bloom_ping", "bloom_ping_image"))
    add_resource(graph_utils.image_view("bloom_pong", "bloom_pong_image"))
    add_resource(graph_utils.image_view("frame_sequence", "frame_sequence_image"))
    add_resource(sampler("bloom_sampler"))
    add_resource(sampler("light_billboard_sampler"))

    -- Tasks the application builds in C++.
    if context.has_mesh_acceleration_structure then
        import_task("build_mesh_acceleration_structure")
    end
    import_task("acquire_frame_sequence")
    import_task("draw_imgui")

    -- The voxel structure, whose overlays draw over the lit scene.
    local voxel = context.voxel
    local overlays = require(voxel.module).add({ imports = imports, resources = resources, tasks = tasks }, voxel, {
        dynamic_mesh_transform_ids = context.dynamic_mesh_transform_ids,
        debug_view_toggles = context.voxel_debug_view_toggles,
        color_target = "swap_chain",
        depth_target = "gbuffer_depth",
    })
    if #overlays == 0 then
        error("deferred: the voxel structure has no overlays.")
    end

    -- Mesh draws. The double-sided and dynamic passes load what the previous pass of their targets wrote.
    local dynamic_transforms = context.dynamic_mesh_transform_ids
    if #dynamic_transforms ~= 2 then
        error("deferred: the dynamic mesh draws two entities, but " .. #dynamic_transforms .. " transforms are supplied.")
    end
    local function draw_mesh(p_id, p_mesh, p_pipeline, p_part, p_load, p_extent, p_clear_values, p_targets, p_aliases, p_transform)
        local load_op = p_load and "load" or "clear"
        local aliases = { render_target_depth = p_targets.depth }
        for index, color in ipairs(p_targets.colors) do
            aliases["render_target_colors[" .. (index - 1) .. "]"] = color
        end
        for key, value in pairs(p_aliases or {}) do
            aliases[key] = value
        end
        tasks.add({
            id = p_id,
            type = "graphics_draw_mesh",
            params = {
                mesh = p_mesh,
                pipeline = p_pipeline,
                parts = { p_part },
                transform = p_transform,
                render_extent = p_extent,
                clear_values = p_clear_values,
                color_load_op = load_op,
                depth_load_op = load_op,
            },
            aliases = aliases,
        })
    end
    local CLEAR_BLACK = { color = { 0.0, 0.0, 0.0, 0.0 } }
    local CLEAR_DEPTH = { depth = 0.0 }
    -- The moments clear to the farthest depth, so unrendered texels never shadow.
    local CLEAR_MOMENTS = { color = { 1.0, 1.0, 0.0, 0.0 } }
    for light = 0, light_count - 1 do
        local targets = {
            colors = { light_id("shadowmap_color", light), light_id("rsm_normal", light), light_id("shadow_moments", light) },
            depth = light_id("shadowmap_depth", light),
        }
        local clear_values = { CLEAR_BLACK, CLEAR_BLACK, CLEAR_MOMENTS, CLEAR_DEPTH }
        local light_aliases = { shadow_light_info = light_id("shadow_light_info", light) }
        draw_mesh(light_id("run_shadow_opaque", light), "static_mesh", "deferred_shadow_opaque", "opaque", false, shadow_extent,
            clear_values, targets, light_aliases)
        draw_mesh(light_id("run_shadow_double_sided", light), "static_mesh", "deferred_shadow_double_sided", "double_sided", true,
            shadow_extent, clear_values, targets, light_aliases)
        draw_mesh(light_id("dynamic_shadow", light), "dynamic_mesh", "deferred_shadow_dynamic", "opaque", true, shadow_extent, {},
            targets, light_aliases, dynamic_transforms[1])
        draw_mesh(light_id("dynamic_shadow_second", light), "dynamic_mesh", "deferred_shadow_dynamic", "opaque", true, shadow_extent,
            {}, targets, light_aliases, dynamic_transforms[2])
    end
    local gbuffer_targets = { colors = { "gbuffer_albedo", "gbuffer_normal", "gbuffer_rma" }, depth = "gbuffer_depth" }
    local gbuffer_clear_values = { CLEAR_BLACK, CLEAR_BLACK, CLEAR_BLACK, CLEAR_DEPTH }
    draw_mesh("run_gbuffer", "static_mesh", "deferred_gbuffer_opaque", "opaque", false, nil, gbuffer_clear_values, gbuffer_targets)
    draw_mesh("run_gbuffer_double_sided", "static_mesh", "deferred_gbuffer_double_sided", "double_sided", true, nil,
        gbuffer_clear_values, gbuffer_targets)
    draw_mesh("dynamic_gbuffer", "dynamic_mesh", "deferred_gbuffer_dynamic", "opaque", true, nil, {}, gbuffer_targets, nil,
        dynamic_transforms[1])
    draw_mesh("dynamic_gbuffer_second", "dynamic_mesh", "deferred_gbuffer_dynamic", "opaque", true, nil, {}, gbuffer_targets, nil,
        dynamic_transforms[2])

    -- Shadow filtering of every light.
    for light = 0, light_count - 1 do
        if summed_area_table then
            tasks.add({
                id = light_id("build_shadow_sat", light),
                type = "run_compute_pipelines_sequential",
                params = {
                    queue_family = "compute_1",
                    dispatches = {
                        { pipeline = "shadow_moments_summed_area_table_horizontal_shader", group_count_x = ceil_div(shadow.height, SUMMED_AREA_TABLE_WORKGROUP_SIZE) },
                        { pipeline = "shadow_moments_summed_area_table_vertical_shader", group_count_x = ceil_div(shadow.width, SUMMED_AREA_TABLE_WORKGROUP_SIZE) },
                    },
                },
                aliases = {
                    shadow_moments = light_id("shadow_moments", light),
                    shadow_sat_horizontal = light_id("shadow_sat_horizontal", light),
                    shadow_sat = light_id("shadow_sat", light),
                },
            })
        else
            tasks.add({
                id = light_id("generate_shadow_mipmaps", light),
                type = "generate_mipmaps",
                params = {
                    source_view = light_id("shadow_moments", light),
                    mip_chain_view = light_id("shadow_moments_sampled", light),
                    base_extent = shadow_extent,
                    mipmap_level_count = context.shadow_mipmap_level_count,
                    format = formats.shadow_moments,
                },
            })
        end
    end

    -- Lighting. Every slot of the lighting shaders' lights[] array maps to a light's shadow resources; slots past the
    -- light count reuse light zero, so that every reflected descriptor binds.
    local light_slots = {}
    if summed_area_table then
        light_slots = { shadow_moments = "shadow_moments", shadow_sat = "shadow_sat", shadowmap_color = "shadowmap_color", rsm_normal = "rsm_normal" }
    else
        light_slots = { shadow_moments_sampled = "shadow_moments_sampled", shadowmap_color = "shadowmap_color", rsm_normal = "rsm_normal" }
    end
    local direct_lighting_aliases = {}
    for slot = 0, context.max_spot_light_info_count - 1 do
        local light = slot < light_count and slot or 0
        for task_resource, graph_resource in pairs(light_slots) do
            direct_lighting_aliases[task_resource .. "[" .. slot .. "]"] = light_id(graph_resource, light)
        end
    end
    local screen_tiles_x = ceil_div(screen.width, SCREEN_TILE_SIZE)
    local screen_tiles_y = ceil_div(screen.height, SCREEN_TILE_SIZE)
    compute("run_direct_lighting", summed_area_table and "direct_lighting_summed_area_table_shader" or "direct_lighting_shader",
        screen_tiles_x, screen_tiles_y, direct_lighting_aliases)
    -- Indirect lighting only loads mip level zero of the moments, so its mip-chain slot binds the base moments view, which
    -- the summed-area-table graph orders without a mip generation pass. The debug view variant replaces it while a voxel
    -- debug view is shown.
    local indirect_lighting_aliases = {}
    for slot = 0, context.max_spot_light_info_count - 1 do
        local light = slot < light_count and slot or 0
        indirect_lighting_aliases["shadow_moments_sampled[" .. slot .. "]"] = light_id("shadow_moments", light)
        indirect_lighting_aliases["shadowmap_color[" .. slot .. "]"] = light_id("shadowmap_color", light)
        indirect_lighting_aliases["rsm_normal[" .. slot .. "]"] = light_id("rsm_normal", light)
    end
    local debug_view_toggles = context.voxel_debug_view_toggles
    for index, pipeline in ipairs({ "indirect_lighting_rsm_shader", "indirect_lighting_rsm_debug_view_shader" }) do
        tasks.add({
            id = INDIRECT_LIGHTING_TASKS[index],
            type = "run_compute_pipeline",
            params = { pipeline = pipeline, group_count_x = screen_tiles_x, group_count_y = screen_tiles_y, queue_family = "compute_2" },
            aliases = indirect_lighting_aliases,
            toggle = index == 1 and debug_view_toggles.disabled or debug_view_toggles.enabled,
        })
    end
    compute("combine_lighting", "combine_lighting_shader", screen_tiles_x, screen_tiles_y)

    -- Post-processing modifies the result image in place after composition.
    local post_processing_last_task = "combine_lighting"
    local post_processing = context.post_processing
    if post_processing ~= nil then
        post_processing_last_task = require(post_processing.module).add(
            { imports = imports, resources = resources, tasks = tasks },
            post_processing,
            { color_view = "result_image", previous_task = "combine_lighting", width = screen.width, height = screen.height })
    end

    -- Bloom, presentation, and on-demand acquisition.
    local bloom_tiles_x = ceil_div(bloom.width, BLOOM_TILE_SIZE)
    local bloom_tiles_y = ceil_div(bloom.height, BLOOM_TILE_SIZE)
    compute("extract_bloom", "bloom_shader_extract", bloom_tiles_x, bloom_tiles_y)
    -- Starting from ping, even iterations blur ping into pong and odd iterations blur pong back into ping, so an even
    -- iteration count leaves the result in ping.
    tasks.add({
        id = "blur_bloom",
        type = "ping_pong",
        params = {
            ping_pipeline = "kawase_blur_pong_write_shader",
            pong_pipeline = "kawase_blur_ping_write_shader",
            iterations = context.bloom_blur_pass_count,
            group_count_x = bloom_tiles_x,
            group_count_y = bloom_tiles_y,
            queue_family = "compute_1",
        },
        aliases = { kawase_blur_ping = "bloom_ping", kawase_blur_pong = "bloom_pong", kawase_blur_sampler = "bloom_sampler" },
    })
    tasks.add({
        id = "acquire_bloom",
        type = "cpu_acquire_image",
        params = { image_view = "bloom_ping" },
        trigger = { event = context.bloom_acquire_event },
    })
    local present_params = { pipeline = "present_post_processing", clear_values = { { color = { 0.0, 0.0, 0.0, 1.0 } } } }
    tasks.add({
        id = "present_result_image",
        type = "run_graphics_pipeline",
        params = present_params,
        aliases = { ["render_target_colors[0]"] = "swap_chain" },
    })
    tasks.add({
        id = "present_frame_sequence",
        type = "run_graphics_pipeline",
        params = present_params,
        aliases = { ["render_target_colors[0]"] = "frame_sequence" },
        trigger = { event = context.frame_sequence_event },
    })
    local billboards = {}
    for light = 0, light_count - 1 do
        billboards[#billboards + 1] = light_id("run_light_billboard", light)
        tasks.add({
            id = billboards[#billboards],
            type = "run_graphics_pipeline",
            params = {
                pipeline = "deferred_light_billboard_pipeline",
                color_load_op = "load",
                depth_load_op = "load",
                draw_vertex_count = 6,
            },
            aliases = {
                light_billboard_info = light_id("light_billboard_info", light),
                ["render_target_colors[0]"] = "swap_chain",
                render_target_depth = "gbuffer_depth",
            },
        })
    end

    -- Shadow and reflective-shadow-map passes of every light.
    for light = 0, light_count - 1 do
        local dynamic_second = light_id("dynamic_shadow_second", light)
        local passes = { light_id("run_shadow_opaque", light), light_id("run_shadow_double_sided", light), light_id("dynamic_shadow", light), dynamic_second }
        for _, target in ipairs({ "shadowmap_color", "rsm_normal", "shadow_moments", "shadowmap_depth" }) do
            chain(light_id(target, light), passes)
        end
        for _, color in ipairs({ "shadowmap_color", "rsm_normal" }) do
            chain(light_id(color, light), { dynamic_second, "run_direct_lighting" })
            chain(light_id(color, light), { dynamic_second, "run_indirect_lighting" })
        end
        local moments = light_id("shadow_moments", light)
        if summed_area_table then
            local build = light_id("build_shadow_sat", light)
            chain(moments, { dynamic_second, build })
            chain(moments, { dynamic_second, "run_direct_lighting" })
            chain(moments, { dynamic_second, "run_indirect_lighting" })
            chain(light_id("shadow_sat", light), { build, "run_direct_lighting" })
        else
            local mipmaps = light_id("generate_shadow_mipmaps", light)
            chain(moments, { dynamic_second, mipmaps, "run_indirect_lighting" })
            chain(light_id("shadow_moments_sampled", light), { mipmaps, "run_direct_lighting" })
        end
    end

    -- G-buffer passes.
    local gbuffer_passes = { "run_gbuffer", "run_gbuffer_double_sided", "dynamic_gbuffer", "dynamic_gbuffer_second", "run_direct_lighting" }
    for _, color in ipairs({ "gbuffer_albedo", "gbuffer_normal", "gbuffer_rma" }) do
        chain(color, gbuffer_passes)
        chain(color, { "dynamic_gbuffer_second", "run_indirect_lighting" })
    end
    chain("gbuffer_rma", { "dynamic_gbuffer_second", "combine_lighting" })
    chain("gbuffer_depth", gbuffer_passes)
    chain("gbuffer_depth", { "dynamic_gbuffer_second", "run_indirect_lighting" })
    -- Overlays depth-test against the G-buffer depth after lighting has sampled it.
    chain("gbuffer_depth", { "run_direct_lighting", overlays[1] })
    local depth_overlays = { "run_indirect_lighting" }
    local swap_chain_users = { "present_result_image" }
    for _, overlay in ipairs(overlays) do
        depth_overlays[#depth_overlays + 1] = overlay
        swap_chain_users[#swap_chain_users + 1] = overlay
    end
    for _, billboard in ipairs(billboards) do
        depth_overlays[#depth_overlays + 1] = billboard
        swap_chain_users[#swap_chain_users + 1] = billboard
    end
    swap_chain_users[#swap_chain_users + 1] = "draw_imgui"
    chain("gbuffer_depth", depth_overlays)

    -- Lighting composition, post-processing, bloom, and presentation.
    chain("direct_lighting", { "run_direct_lighting", "combine_lighting" })
    chain("indirect_lighting", { "run_indirect_lighting", "combine_lighting" })
    chain("result_image", { post_processing_last_task, "extract_bloom" })
    chain("result_image", { post_processing_last_task, "present_result_image" })
    -- The blur task ping-pongs between the bloom images internally; only its final ping result leaves the task.
    chain("bloom_ping", { "extract_bloom", "blur_bloom", "acquire_bloom", "present_result_image" })
    -- The frame sequence presents the same result and bloom into its own image, which it then acquires.
    chain("result_image", { post_processing_last_task, "present_frame_sequence" })
    chain("bloom_ping", { "blur_bloom", "present_frame_sequence" })
    chain("frame_sequence", { "present_frame_sequence", "acquire_frame_sequence" })
    chain("swap_chain", swap_chain_users)

    return { imports = imports, resources = resources, tasks = tasks.tasks }
end

return exports
