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

-- Sparse voxel octree construction and debug views, for recipes that include it as a feature: the atomic-publication
-- ("atomic") and pass-publication ("pass_published") implementations. The application compiles the pipelines and owns
-- the SVO buffers with initial contents or state both deferred graphs share, which every graph imports; the module
-- declares the scratch buffers each frame rewrites before reading. See ISVORenderFeature in
-- tests/executable/function/render/deferred_test/i_svo_render_feature.hh.

local type wbe = require("wbe")
local type voxel = require("render_graphs.features.voxel")
local graph_utils = require("utils.render_graph")

local voxel_svo = {}

-- The SVO as a subgraph task with id p_id, whose overlays draw on its `color` and `depth` ports, bound to the target's.
function voxel_svo.subgraph(p_id: string, p_params: wbe.SVORecipeParams, p_target: voxel.Target): graph_utils.Task
    local variant = graph_utils.enum_name(p_params.variant)
    local pass_published = variant == "PASS_PUBLISHED"
    if not pass_published and variant ~= "ATOMIC" then
        error("voxel_svo: unknown variant '" .. variant .. "'.")
    end
    -- Scratch buffers: each frame's dynamic clear (and the static reset) empties the dynamic nodes' range and the
    -- deferred lists before the construction fills them, and the pre-deferred capture rewrites its copies before the
    -- debug draw reads them.
    local buffers: {string:graph_utils.Resource} = {}
    for _, id in ipairs(p_params.imports) do
        buffers[id] = graph_utils.import_resource(id)
    end
    local sizes = p_params.buffer_sizes
    buffers.svo_dynamic_nodes = graph_utils.storage_buffer("svo_dynamic_nodes", sizes.dynamic_nodes)
    buffers.svo_deferred_list_ping = graph_utils.storage_buffer("svo_deferred_list_ping", sizes.deferred_list)
    buffers.svo_deferred_list_pong = graph_utils.storage_buffer("svo_deferred_list_pong", sizes.deferred_list)
    buffers.svo_pre_deferred_debug_info_buffer =
        graph_utils.storage_buffer("svo_pre_deferred_debug_info_buffer", sizes.pre_deferred_debug_info)
    buffers.svo_pre_deferred_debug_list_buffer =
        graph_utils.storage_buffer("svo_pre_deferred_debug_list_buffer", sizes.pre_deferred_debug_list)
    local color = graph_utils.port("color")
    local depth = graph_utils.port("depth")
    local graph = graph_utils.graph()
    local prefix = p_params.shader_prefix
    local dynamic_prefix = p_params.dynamic_shader_prefix
    local toggles = p_target.debug_view_toggles
    local static_build = p_params.static_build
    local extent: wbe.RendExtent2DParamsVK = { width = p_params.extent, height = p_params.extent }

    local function compute(p_task_id: string, p_pipeline: string, p_group_count_x: integer): graph_utils.Task
        local params: wbe.RTParamsRunComputePipelineVK =
            { pipeline = p_pipeline, group_count_x = p_group_count_x, queue_family = "compute_1" }
        return graph:compute(p_task_id, params)
    end
    local function construction(p_task_id: string, p_mesh: string, p_pipeline: string, p_part: wbe.MeshPartVK,
        p_transform: graph_utils.Resource, p_load: boolean): graph_utils.Task
        local load_op: wbe.AttachmentLoadOpVK = p_load and "load" or "clear"
        local params: graph_utils.DrawMeshParams = {
            mesh = p_mesh,
            pipeline = p_pipeline,
            parts = { p_part },
            transform = p_transform,
            render_extent = extent,
            color_load_op = load_op,
            depth_load_op = load_op,
        }
        return graph:draw_mesh(p_task_id, params)
    end
    -- Rasterization allocates depth one's children; every propagation round publishes one further level, and an extra
    -- retry consumes leaf occupancy and drains the remaining deferred entries.
    local function deferred_construction(p_task_id: string, p_dynamic: boolean): graph_utils.Task
        local stage_prefix = p_dynamic and dynamic_prefix or prefix
        local ping = stage_prefix .. "_build_deferred_propagation_pong_write_shader"
        local pong = stage_prefix .. "_build_deferred_propagation_ping_write_shader"
        local task: graph_utils.Task
        if pass_published then
            local publication = p_dynamic and "svo_2_dynamic_update_states_shader" or "svo_2_update_states_shader"
            local publication_group_count =
                p_dynamic and p_params.dynamic_publication_group_count or p_params.static_publication_group_count
            local dispatches: {graph_utils.ComputeDispatchParams} = {}
            for iteration = 0, p_params.max_depth - 1 do
                local write_pong = iteration % 2 == 0
                dispatches[#dispatches + 1] = {
                    pipeline = write_pong and "svo_deferred_count_reset_pong_shader" or "svo_deferred_count_reset_ping_shader",
                }
                dispatches[#dispatches + 1] = { pipeline = write_pong and ping or pong, group_count_x = p_params.deferred_dispatch_group_count }
                dispatches[#dispatches + 1] = { pipeline = publication, group_count_x = publication_group_count }
            end
            local params: graph_utils.ComputeSequentialParams = { dispatches = dispatches, queue_family = "compute_1" }
            task = graph:compute_sequential(p_task_id, params)
        else
            -- A single raster pass can leave contenders at any depth; allow one retry per tree level.
            local params: wbe.RTParamsPingPongVK = {
                ping_pipeline = ping,
                pong_pipeline = pong,
                iterations = p_params.max_depth,
                group_count_x = p_params.deferred_dispatch_group_count,
                ping_prefix_pipeline = "svo_deferred_count_reset_pong_shader",
                pong_prefix_pipeline = "svo_deferred_count_reset_ping_shader",
                queue_family = "compute_1",
            }
            task = graph:ping_pong(p_task_id, params)
        end
        return task
    end
    local function overlay(p_task_id: string, p_pipeline: string, p_indirect_buffer: graph_utils.Resource,
        p_toggle: wbe.RendRecipeToggleVK): graph_utils.Task
        local params: graph_utils.DrawIndirectParams = {
            pipeline = p_pipeline,
            index_buffer = buffers.svo_debug_draw_index_buffer,
            indirect_buffer = p_indirect_buffer,
            color_load_op = "load",
            depth_load_op = "load",
        }
        local task = graph:draw_indirect(p_task_id, params, {
            toggle = p_toggle,
        })
        task:color_target(0, color)
        task:depth_target(depth)
        return task
    end
    -- The frame-begin barrier orders this frame's SVO mutation after the previous frame's consumers.
    local barrier_params: graph_utils.FullMemoryBarrierParams = { resource = buffers.svo_info }
    local frame_begin = graph:full_memory_barrier("svo_frame_begin", barrier_params)
    -- Static scene, built once into the shared buffers unless the application rebuilds it every frame (no trigger).
    local reset = compute("reset_svo", prefix .. "_reset_shader", p_params.reset_group_count)
    reset.trigger = static_build
    local build = construction("run_build_svo", "static_mesh", prefix .. "_build_static_mesh_pipeline", "opaque", nil, false)
    build.trigger = static_build
    local build_double_sided = construction("run_build_svo_double_sided", "static_mesh", prefix .. "_build_static_mesh_pipeline", "double_sided", nil,
        false)
    build_double_sided.trigger = static_build
    local static_publication: {graph_utils.Task} = {}
    if pass_published then
        local update_states = compute("update_svo_states", "svo_2_update_states_shader", p_params.static_publication_group_count)
        update_states.trigger = static_build
        static_publication = { update_states }
    end
    local static_deferred = deferred_construction("static_svo_deferred", false)
    static_deferred.trigger = static_build
    -- Dynamic meshes.
    local clear_dynamic = compute("clear_dynamic_svo", dynamic_prefix .. "_clear_shader", p_params.dynamic_clear_group_count)
    local dynamic: {graph_utils.Task} = {}
    for index, id in ipairs({ "dynamic_svo", "dynamic_svo_second" }) do
        dynamic[index] = construction(id, "dynamic_mesh", dynamic_prefix .. "_build_static_mesh_pipeline", "opaque",
            graph_utils.reference(p_target.dynamic_mesh_transform_ids[index]), true)
    end
    local dynamic_publication: {graph_utils.Task} = {}
    if pass_published then
        dynamic_publication = { compute("publish_dynamic_svo", "svo_2_dynamic_update_states_shader", p_params.dynamic_publication_group_count) }
    end
    local capture_pre_deferred_debug = compute("capture_svo_pre_deferred_debug", "svo_pre_deferred_debug_capture_shader", p_params.deferred_dispatch_group_count)
    capture_pre_deferred_debug.toggle = toggles.pre_deferred_propagation
    local dynamic_deferred = deferred_construction("run_build_svo_deferred", true)
    local setup_debug_draw = compute("setup_svo_debug_draw", "svo_debug_draw_setup_shader", 1)
    -- Debug views.
    local debug_draw = overlay("run_svo_debug_draw", "svo_debug_draw_pipeline", buffers.svo_debug_draw_indirect_buffer, toggles.post_svo_build)
    local pre_deferred_debug_draw = overlay("run_svo_pre_deferred_debug_draw", "svo_pre_deferred_debug_draw_pipeline", buffers.svo_pre_deferred_debug_draw_indirect_buffer,
        toggles.pre_deferred_propagation)

    -- Every task using a buffer, in execution order; the publication passes of the pass-published implementation take
    -- their place in the chains of the buffers they publish.
    local function sequence(...: {graph_utils.Task}): {graph_utils.Task}
        local result: {graph_utils.Task} = {}
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
        { capture_pre_deferred_debug, dynamic_deferred, setup_debug_draw, debug_draw }))
    graph:chain(buffers.svo_node_buffer, sequence({ reset, build, build_double_sided }, static_publication,
        { static_deferred, clear_dynamic, dynamic[1], dynamic[2] }, dynamic_publication,
        { dynamic_deferred, debug_draw, pre_deferred_debug_draw }))
    graph:chain({ buffers.svo_dynamic_nodes, buffers.svo_dynamic_leaves },
        sequence({ clear_dynamic, dynamic[1], dynamic[2] }, dynamic_publication,
            { dynamic_deferred, debug_draw, pre_deferred_debug_draw }))
    graph:chain(buffers.svo_leaf_buffer, {
        build,
        build_double_sided,
        static_deferred,
        dynamic[1],
        dynamic[2],
        dynamic_deferred,
        debug_draw,
        pre_deferred_debug_draw,
    })
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
    graph:chain(buffers.svo_debug_draw_indirect_buffer, { setup_debug_draw, debug_draw })
    -- The overlays draw in this order.
    graph:chain({ color, depth }, { debug_draw, pre_deferred_debug_draw })
    return graph_utils.subgraph(p_id, graph:build(p_params.imports, { "color", "depth" }),
        { color = p_target.color_target, depth = p_target.depth_target })
end

return voxel_svo
