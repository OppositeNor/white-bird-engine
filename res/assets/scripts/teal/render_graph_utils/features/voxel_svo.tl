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

-- Sparse voxel octree construction, SVOGI injection, and debug views, for recipes that include it as a feature: the
-- atomic-publication ("atomic") and pass-publication ("pass_published") implementations. After each frame's construction
-- the occupancy injection writes the tree's occupancy, which propagates up to the root's sectors, and the radiance
-- injection stages the reflective shadow maps' flux, unreduced, as one entry per sample and light in the leaf it falls
-- in. The radiance injection binds the target's reflective-shadow-map slots, so the including recipe orders it after the
-- lights' shadow passes through edges to the subgraph. The application compiles the pipelines and owns
-- the SVO buffers with initial contents or state both deferred graphs share, which every graph imports; the module
-- declares the scratch buffers each frame rewrites before reading. See ISVORenderFeature in
-- tests/executable/function/render/deferred_test/i_svo_render_feature.hh.

local type wbe = require("wbe")
local type voxel = require("render_graph_utils.features.voxel")
local render_graph = require("render_graph_utils.render_graph_utils")

local record voxel_svo
    -- The subgraph injects radiance from the target's reflective-shadow-map slots.
    injects_radiance: boolean
end

voxel_svo.injects_radiance = true

-- The SVO as a subgraph task with id identifier, whose overlays draw on its `color` and `depth` ports, bound to the target's.
function voxel_svo.subgraph(identifier: string, parameters: wbe.SVORecipeParams, target: voxel.Target): render_graph.Task
    local variant = render_graph.enum_name(parameters.variant)
    local pass_published = variant == "PASS_PUBLISHED"
    if not pass_published and variant ~= "ATOMIC" then
        error("voxel_svo: unknown variant '" .. variant .. "'.")
    end
    -- Scratch buffers: each frame's dynamic clear (and the static reset) empties the dynamic nodes' range and the
    -- deferred lists before the construction fills them, and the pre-deferred capture rewrites its copies before the
    -- debug draw reads them.
    local graph = render_graph.new()
    local buffers: {string:render_graph.Resource} = {}
    for _, id in ipairs(parameters.imports) do
        buffers[id] = graph:import_resource(id)
    end
    local sizes = parameters.buffer_sizes
    buffers.svo_deferred_list_ping = graph:add_storage_buffer("svo_deferred_list_ping", sizes.deferred_list)
    buffers.svo_deferred_list_pong = graph:add_storage_buffer("svo_deferred_list_pong", sizes.deferred_list)
    buffers.svo_pre_deferred_debug_info_buffer =
        graph:add_storage_buffer("svo_pre_deferred_debug_info_buffer", sizes.pre_deferred_debug_info)
    buffers.svo_pre_deferred_debug_list_buffer =
        graph:add_storage_buffer("svo_pre_deferred_debug_list_buffer", sizes.pre_deferred_debug_list)
    -- The SVOGI injection rewrites these every frame: the occupancy of every occupied leaf and every node with children,
    -- and the radiance entries after the count reset.
    buffers.svogi_occupancy_buffer = graph:add_storage_buffer("svogi_occupancy_buffer", sizes.occupancy)
    buffers.svogi_radiance_index_buffer = graph:add_storage_buffer("svogi_radiance_index_buffer", sizes.radiance_index)
    buffers.svogi_radiance_staged_buffer = graph:add_storage_buffer("svogi_radiance_staged_buffer", sizes.radiance_staged)
    -- The post-construction view's node list, rewritten each frame at a node depth before the draw reads it.
    buffers.svo_debug_draw_node_list = graph:add_storage_buffer("svo_debug_draw_node_list", sizes.debug_draw_node_list)
    local color = graph:port("color")
    local depth = graph:port("depth")
    local prefix = parameters.shader_prefix
    local dynamic_prefix = parameters.dynamic_shader_prefix
    local static_mesh = graph:add_mesh("static_mesh")
    local dynamic_mesh = graph:add_mesh("dynamic_mesh")
    local compute_pipelines: {string:render_graph.Resource} = {
        reset = render_graph.shader_compute_pipeline(prefix .. "_reset_shader"),
        clear_dynamic = render_graph.shader_compute_pipeline(dynamic_prefix .. "_clear_shader"),
        static_ping = render_graph.shader_compute_pipeline(prefix .. "_build_deferred_propagation_pong_write_shader"),
        static_pong = render_graph.shader_compute_pipeline(prefix .. "_build_deferred_propagation_ping_write_shader"),
        dynamic_ping = render_graph.shader_compute_pipeline(dynamic_prefix .. "_build_deferred_propagation_pong_write_shader"),
        dynamic_pong = render_graph.shader_compute_pipeline(dynamic_prefix .. "_build_deferred_propagation_ping_write_shader"),
        static_publication = render_graph.shader_compute_pipeline("svo_2_update_states_shader"),
        dynamic_publication = render_graph.shader_compute_pipeline("svo_2_dynamic_update_states_shader"),
        reset_ping = render_graph.shader_compute_pipeline("svo_deferred_count_reset_pong_shader"),
        reset_pong = render_graph.shader_compute_pipeline("svo_deferred_count_reset_ping_shader"),
        capture_pre_deferred_debug = render_graph.shader_compute_pipeline("svo_pre_deferred_debug_capture_shader"),
        setup_debug_draw = render_graph.shader_compute_pipeline("svo_debug_draw_setup_shader"),
        find_debug_draw_nodes = render_graph.shader_compute_pipeline("svo_debug_draw_find_nodes_shader"),
        occupancy_injection = render_graph.shader_compute_pipeline("svogi_occupancy_injection"),
        occupancy_propagation = render_graph.shader_compute_pipeline("svogi_occupancy_propagation"),
        radiance_injection = render_graph.shader_compute_pipeline("svogi_radiance_injection"),
    }
    -- The constructions rasterize without attachments; the tree draws draw the cubes' edges, or their faces in the solid
    -- styles, onto the target.
    local function construction_pipeline(key: string): render_graph.Resource
        return render_graph.graphics_pipeline(key, { pipeline = key })
    end
    local function overlay_pipeline(key: string, topology: wbe.PrimitiveTopologyVK): render_graph.Resource
        return render_graph.graphics_pipeline(key, {
            pipeline = key,
            color_formats = { target.color_format },
            depth_format = target.depth_format,
            primitive_topology = topology,
        })
    end
    local graphics_pipelines: {string:render_graph.Resource} = {
        static_construction = construction_pipeline(prefix .. "_build_static_mesh_pipeline"),
        dynamic_construction = construction_pipeline(dynamic_prefix .. "_build_static_mesh_pipeline"),
        -- The tree draw's branches, in the order the application selects them: the VoxelDebugDrawStyle, plus two in the
        -- occupancy view.
        tree_draw_wireframe = overlay_pipeline("svo_debug_draw_pipeline", "LINE_LIST"),
        tree_draw_solid = overlay_pipeline("svo_debug_draw_solid_pipeline", "TRIANGLE_LIST"),
        tree_draw_wireframe_occupancy = overlay_pipeline("svo_debug_draw_occupancy_pipeline", "LINE_LIST"),
        tree_draw_solid_occupancy = overlay_pipeline("svo_debug_draw_solid_occupancy_pipeline", "TRIANGLE_LIST"),
        pre_deferred_debug_draw = overlay_pipeline("svo_pre_deferred_debug_draw_pipeline", "LINE_LIST"),
    }
    local toggles = target.debug_view_toggles
    local static_build = parameters.static_build
    local extent: wbe.RendExtent2DParamsVK = { width = parameters.extent, height = parameters.extent }

    local function compute(task_id: string, pipeline: render_graph.Resource, group_count_x: integer): render_graph.Task
        local params: render_graph.RTParamsRunComputePipelineVK =
            { pipeline = pipeline, group_count_x = group_count_x, queue_family = "compute_1" }
        return graph:compute(task_id, params)
    end
    local function construction(task_id: string, mesh: render_graph.Resource, pipeline: render_graph.Resource, mesh_part: wbe.MeshPartVK,
        transform: render_graph.Resource, load_existing: boolean): render_graph.Task
        local load_op: wbe.AttachmentLoadOpVK = load_existing and "load" or "clear"
        local params: render_graph.RTParamsGraphicsDrawMeshVK = {
            mesh = mesh,
            pipeline = { default = pipeline },
            parts = { mesh_part },
            transform = transform,
            render_extent = extent,
            color_load_op = load_op,
            depth_load_op = load_op,
        }
        return graph:draw_mesh(task_id, params)
    end
    -- Rasterization allocates depth one's children; every propagation round publishes one further level, and an extra
    -- retry consumes leaf occupancy and drains the remaining deferred entries.
    local function deferred_construction(task_id: string, is_dynamic: boolean): render_graph.Task
        local ping = is_dynamic and compute_pipelines.dynamic_ping or compute_pipelines.static_ping
        local pong = is_dynamic and compute_pipelines.dynamic_pong or compute_pipelines.static_pong
        local task: render_graph.Task
        if pass_published then
            local publication = is_dynamic and compute_pipelines.dynamic_publication or compute_pipelines.static_publication
            local publication_group_count =
                is_dynamic and parameters.dynamic_publication_group_count or parameters.static_publication_group_count
            local dispatches: {render_graph.RTParamsComputeDispatchVK} = {}
            for iteration = 0, parameters.max_depth - 1 do
                local write_pong = iteration % 2 == 0
                dispatches[#dispatches + 1] = {
                    pipeline = write_pong and compute_pipelines.reset_ping or compute_pipelines.reset_pong,
                }
                dispatches[#dispatches + 1] = { pipeline = write_pong and ping or pong, group_count_x = parameters.deferred_dispatch_group_count }
                dispatches[#dispatches + 1] = { pipeline = publication, group_count_x = publication_group_count }
            end
            local params: render_graph.RTParamsRunComputePipelinesSequentialVK = { dispatches = dispatches, queue_family = "compute_1" }
            task = graph:compute_sequential(task_id, params)
        else
            -- A single raster pass can leave contenders at any depth; allow one retry per tree level.
            local params: render_graph.RTParamsPingPongVK = {
                ping_pipeline = ping,
                pong_pipeline = pong,
                iterations = parameters.max_depth,
                group_count_x = parameters.deferred_dispatch_group_count,
                ping_prefix_pipeline = compute_pipelines.reset_ping,
                pong_prefix_pipeline = compute_pipelines.reset_pong,
                queue_family = "compute_1",
            }
            task = graph:ping_pong(task_id, params)
        end
        return task
    end
    local function overlay(task_id: string, pipeline: render_graph.Resource, indirect_buffer: render_graph.Resource,
        toggle: wbe.RendRecipeToggleVK): render_graph.Task
        local params: render_graph.RTParamsRunGraphicsPipelineIndirectVK = {
            pipeline = pipeline,
            index_buffer = buffers.svo_debug_draw_index_buffer,
            indirect_buffer = indirect_buffer,
            color_load_op = "load",
            depth_load_op = "load",
        }
        local task = graph:draw_indirect(task_id, params, {
            toggle = toggle,
        })
        task:color_target(0, color)
        task:depth_target(depth)
        return task
    end
    -- The frame-begin barrier orders this frame's SVO mutation after the previous frame's consumers.
    local barrier_params: render_graph.RTParamsFullMemoryBarrierVK = { resource = buffers.svo_info }
    local frame_begin = graph:full_memory_barrier("svo_frame_begin", barrier_params)
    -- Static scene, built once into the shared buffers unless the application rebuilds it every frame (no trigger).
    local reset = compute("reset_svo", compute_pipelines.reset, parameters.reset_group_count)
    reset.trigger = static_build
    local build = construction("run_build_svo", static_mesh, graphics_pipelines.static_construction, "opaque", nil, false)
    build.trigger = static_build
    local build_double_sided = construction("run_build_svo_double_sided", static_mesh, graphics_pipelines.static_construction, "double_sided", nil,
        false)
    build_double_sided.trigger = static_build
    local static_publication: {render_graph.Task} = {}
    if pass_published then
        local update_states = compute("update_svo_states", compute_pipelines.static_publication, parameters.static_publication_group_count)
        update_states.trigger = static_build
        static_publication = { update_states }
    end
    local static_deferred = deferred_construction("static_svo_deferred", false)
    static_deferred.trigger = static_build
    -- Dynamic meshes.
    local clear_dynamic = compute("clear_dynamic_svo", compute_pipelines.clear_dynamic, parameters.dynamic_clear_group_count)
    local dynamic: {render_graph.Task} = {}
    for index, id in ipairs({ "dynamic_svo", "dynamic_svo_second" }) do
        dynamic[index] = construction(id, dynamic_mesh, graphics_pipelines.dynamic_construction, "opaque",
            graph:reference(target.dynamic_mesh_transform_ids[index]), true)
    end
    local dynamic_publication: {render_graph.Task} = {}
    if pass_published then
        dynamic_publication = { compute("publish_dynamic_svo", compute_pipelines.dynamic_publication, parameters.dynamic_publication_group_count) }
    end
    local capture_pre_deferred_debug = compute("capture_svo_pre_deferred_debug", compute_pipelines.capture_pre_deferred_debug, parameters.deferred_dispatch_group_count)
    capture_pre_deferred_debug.toggle = toggles.pre_deferred_propagation
    local dynamic_deferred = deferred_construction("run_build_svo_deferred", true)
    -- SVOGI injection into the completed tree. The occupancy injection writes the leaves and their parents; propagation
    -- then writes one level further up per dispatch, from the parents of the leaves' parents to the root's sectors.
    local occupancy_injection_params: render_graph.RTParamsRunComputePipelineVK = {
        pipeline = compute_pipelines.occupancy_injection,
        group_count_x = parameters.occupancy_injection_group_count_x,
        group_count_y = parameters.occupancy_injection_group_count_y,
        queue_family = "compute_1",
    }
    local occupancy_injection = graph:compute("inject_svogi_occupancy", occupancy_injection_params, {
        bindings = { atlas = buffers.svogi_occupancy_buffer },
    })
    local propagation_dispatches: {render_graph.RTParamsComputeDispatchVK} = {}
    for child_depth = parameters.max_depth - 1, 2, -1 do
        propagation_dispatches[#propagation_dispatches + 1] = {
            pipeline = compute_pipelines.occupancy_propagation,
            group_count_x = parameters.occupancy_propagation_group_count,
            push_constants = { child_depth = child_depth },
        }
    end
    local propagation_params: render_graph.RTParamsRunComputePipelinesSequentialVK =
        { dispatches = propagation_dispatches, queue_family = "compute_1" }
    local occupancy_propagation = graph:compute_sequential("propagate_svogi_occupancy", propagation_params, {
        bindings = { atlas = buffers.svogi_occupancy_buffer },
    })
    -- The radiance index buffer is a counted buffer, whose element byte size shares the deferred lists' count element; the
    -- pong list's count reset clears it.
    local reset_radiance_index_params: render_graph.RTParamsRunComputePipelineVK =
        { pipeline = compute_pipelines.reset_ping, queue_family = "compute_1" }
    local reset_radiance_index = graph:compute("reset_svogi_radiance_index", reset_radiance_index_params, {
        bindings = { svo_deferred_list_pong = buffers.svogi_radiance_index_buffer },
    })
    local radiance_bindings: {string:render_graph.Resource} = {
        index_buffer = buffers.svogi_radiance_index_buffer,
        staged_buffer = buffers.svogi_radiance_staged_buffer,
    }
    for slot, id in pairs(target.rsm_slot_ids) do
        radiance_bindings[slot] = graph:reference(id)
    end
    local radiance_injection_params: render_graph.RTParamsRunComputePipelineVK = {
        pipeline = compute_pipelines.radiance_injection,
        group_count_x = parameters.radiance_injection_group_count,
        group_count_y = parameters.radiance_injection_group_count,
        queue_family = "compute_1",
    }
    local radiance_injection = graph:compute("inject_svogi_radiance", radiance_injection_params, { bindings = radiance_bindings })
    local setup_debug_draw = compute("setup_svo_debug_draw", compute_pipelines.setup_debug_draw, 1)
    -- The post-construction view's nodes at the depth svo_debug_draw_settings selects; the setup writes the root's sectors.
    local find_debug_draw_nodes = compute("find_svo_debug_draw_nodes", compute_pipelines.find_debug_draw_nodes,
        parameters.debug_draw_node_search_group_count)
    find_debug_draw_nodes.toggle = toggles.tree_view
    -- Debug views.
    -- The post-construction tree, drawn by the post-SVO-build and occupancy views: one multiplexer of every style and
    -- coloring, whose branches share the indirect command and the targets. The solid styles index the cube's faces.
    local branch_graph = render_graph.new()
    local tree_draw_branches: {render_graph.Task} = {}
    for _, branch in ipairs({
        { pipeline = graphics_pipelines.tree_draw_wireframe, index_buffer = buffers.svo_debug_draw_index_buffer },
        { pipeline = graphics_pipelines.tree_draw_solid, index_buffer = buffers.svo_debug_draw_solid_index_buffer },
        { pipeline = graphics_pipelines.tree_draw_wireframe_occupancy, index_buffer = buffers.svo_debug_draw_index_buffer },
        { pipeline = graphics_pipelines.tree_draw_solid_occupancy, index_buffer = buffers.svo_debug_draw_solid_index_buffer },
    }) do
        local params: render_graph.RTParamsRunGraphicsPipelineIndirectVK = {
            pipeline = branch.pipeline,
            index_buffer = branch.index_buffer,
            indirect_buffer = buffers.svo_debug_draw_indirect_buffer,
            color_load_op = "load",
            depth_load_op = "load",
        }
        tree_draw_branches[#tree_draw_branches + 1] =
            branch_graph:draw_indirect("tree_draw_" .. #tree_draw_branches, params):color_target(0, color):depth_target(depth)
    end
    local debug_draw = graph:multiplex("run_svo_debug_draw", tree_draw_branches, {
        select = target.debug_view_toggles.tree_draw,
        toggle = toggles.tree_view,
    })
    local pre_deferred_debug_draw = overlay("run_svo_pre_deferred_debug_draw", graphics_pipelines.pre_deferred_debug_draw, buffers.svo_pre_deferred_debug_draw_indirect_buffer,
        toggles.pre_deferred_propagation)

    -- The injection reads the completed tree; it reaches the graphics queue through the overlays that follow it.
    local injection = { occupancy_injection, occupancy_propagation, radiance_injection }
    -- Every task using a buffer, in execution order; the publication passes of the pass-published implementation take
    -- their place in the chains of the buffers they publish.
    local function sequence(...: {render_graph.Task}): {render_graph.Task}
        local result: {render_graph.Task} = {}
        for _, part in ipairs({ ... }) do
            for _, task in ipairs(part) do
                result[#result + 1] = task
            end
        end
        return result
    end
    graph:chain(buffers.svo_info, { frame_begin, reset })
    graph:chain(buffers.svo_info, sequence({ reset, build, build_double_sided }, static_publication,
        { static_deferred, clear_dynamic, dynamic[1], dynamic[2] }, dynamic_publication,
        { capture_pre_deferred_debug, dynamic_deferred }, injection, { setup_debug_draw, find_debug_draw_nodes, debug_draw }))
    graph:chain(buffers.svo_node_buffer, sequence({ reset, build, build_double_sided }, static_publication,
        { static_deferred, clear_dynamic, dynamic[1], dynamic[2] }, dynamic_publication,
        { dynamic_deferred }, injection, { setup_debug_draw, find_debug_draw_nodes, debug_draw, pre_deferred_debug_draw }))
    graph:chain(buffers.svo_leaf_buffer, sequence({
        build,
        build_double_sided,
        static_deferred,
        dynamic[1],
        dynamic[2],
        dynamic_deferred,
    }, injection, { find_debug_draw_nodes, debug_draw, pre_deferred_debug_draw }))
    graph:chain(buffers.svo_deferred_list_ping, {
        reset,
        build,
        build_double_sided,
        static_deferred,
        clear_dynamic,
        dynamic[1],
        dynamic[2],
        capture_pre_deferred_debug,
        dynamic_deferred,
    })
    graph:chain(buffers.svo_deferred_list_pong, { reset, static_deferred, clear_dynamic, dynamic_deferred })
    graph:chain({ buffers.svo_pre_deferred_debug_info_buffer, buffers.svo_pre_deferred_debug_list_buffer,
        buffers.svo_pre_deferred_debug_draw_indirect_buffer }, { capture_pre_deferred_debug, pre_deferred_debug_draw })
    graph:chain({ buffers.svo_debug_draw_indirect_buffer, buffers.svo_debug_draw_node_list },
        { setup_debug_draw, find_debug_draw_nodes, debug_draw })
    -- The occupancy view reads the propagated occupancy.
    graph:chain(buffers.svogi_occupancy_buffer, { occupancy_injection, occupancy_propagation, debug_draw })
    graph:chain(buffers.svogi_radiance_index_buffer, { reset_radiance_index, radiance_injection })
    -- The overlays draw in this order.
    graph:chain({ color, depth }, { debug_draw, pre_deferred_debug_draw })
    return graph:as_subgraph(identifier, { color = target.color_target, depth = target.depth_target }, parameters.imports)
end

return voxel_svo
