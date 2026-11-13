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
-- lights' shadow passes through edges to the subgraph. The module registers its pipelines; the application owns
-- the SVO buffers with initial contents or state both deferred graphs share, which every graph imports by the names the
-- shaders reflect, and the module declares the scratch buffers each frame rewrites before reading. See ISVORenderFeature in
-- tests/executable/function/render/deferred_test/i_svo_render_feature.hh.
local sort = require("render_graph_utils.sort")

local type wbe = require("wbe")
local type voxel = require("render_graph_utils.features.voxel")
local graph = require("render_graph_utils.render_graph_utils")

local record voxel_svo
    -- The subgraph injects radiance from the target's reflective-shadow-map slots.
    injects_radiance: boolean
end

voxel_svo.injects_radiance = true

-- The subgraph's tasks, by role. The publication lists are empty in the atomic implementation.
local record Tasks
    frame_begin: graph.Task
    reset: graph.Task
    build: graph.Task
    build_double_sided: graph.Task
    static_publication: {graph.Task}
    static_deferred: graph.Task
    clear_dynamic: graph.Task
    dynamic: {graph.Task}
    dynamic_publication: {graph.Task}
    capture_pre_deferred_debug: graph.Task
    dynamic_deferred: graph.Task
    occupancy_injection: graph.Task
    occupancy_propagation: graph.Task
    reset_radiance_index: graph.Task
    radiance_injection: graph.Task
    setup_debug_draw: graph.Task
    find_debug_draw_nodes: graph.Task
    debug_draw: graph.Task
    pre_deferred_debug_draw: graph.Task
    radiance_sort: graph.Task
end

-- The subgraph under construction, shared by the build steps.
local record Builder
    parameters: wbe.SVORecipeParams
    target: voxel.Target
    -- Whether this is the pass-published implementation, rather than the atomic one.
    pass_published: boolean
    -- The resources the shaders use, by the names they reflect.
    buffers: {string:graph.Resource}
    color: graph.Resource
    depth: graph.Resource
    static_mesh: graph.Resource
    dynamic_mesh: graph.Resource
    compute_pipelines: {string:graph.Resource}
    graphics_pipelines: {string:graph.Resource}
    -- The render area of the construction rasterization.
    extent: wbe.RendExtent2DParamsVK
    tasks: Tasks
end

-- Whether parameters select the pass-published implementation; fails on an unknown variant.
local function is_pass_published(parameters: wbe.SVORecipeParams): boolean
    local variant = graph.enum_name(parameters.variant)
    local pass_published = variant == "PASS_PUBLISHED"
    if not pass_published and variant ~= "ATOMIC" then
        error("voxel_svo: unknown variant '" .. variant .. "'.")
    end
    return pass_published
end

-- The application's buffers, the scratch buffers, and the enclosing graph's camera and lights, by the names the shaders
-- reflect. Each frame's dynamic clear (and the static reset) empties the dynamic nodes' range and the deferred lists before
-- the construction fills them, and the pre-deferred capture rewrites its copies before the debug draw reads them.
local function create_buffers(parameters: wbe.SVORecipeParams): {string:graph.Resource}
    local buffers: {string:graph.Resource} = {}
    for _, id in ipairs({ "svo_info", "svo_node_buffer", "svo_leaf_buffer", "svo_debug_draw_index_buffer",
        "svo_debug_draw_indirect_buffer", "svo_pre_deferred_debug_draw_indirect_buffer", "svo_debug_draw_settings",
        "svo_debug_draw_solid_index_buffer", "gbuffer_camera_info", "spot_light_infos" }) do
        buffers[id] = graph.import(id)
    end
    local sizes = parameters.buffer_sizes
    local function scratch(name: string, size: integer)
        buffers[name] = graph.storage_buffer({ name = name, size = size })
    end
    scratch("svo_deferred_list_ping", sizes.deferred_list)
    scratch("svo_deferred_list_pong", sizes.deferred_list)
    scratch("svo_pre_deferred_debug_info_buffer", sizes.pre_deferred_debug_info)
    scratch("svo_pre_deferred_debug_list_buffer", sizes.pre_deferred_debug_list)
    -- The SVOGI injection rewrites these every frame: the occupancy of every occupied leaf and every node with children,
    -- and the radiance entries after the count reset.
    scratch("svogi_occupancy_buffer", sizes.occupancy)
    scratch("svogi_radiance_index_buffer", sizes.radiance_index)
    scratch("svogi_radiance_staged_buffer", sizes.radiance_staged)
    -- TODO(4): Declare the reduced radiance buffer here (one radiance value, and optionally a sample count, per leaf
    -- subvoxel, indexed by data index like svogi_occupancy_buffer), plus the sort and scan scratch buffers from
    -- sort.radix_sort_buffers() and large_prefix_scan.buffers(). Add their sizes to SVORecipeParams.buffer_sizes.
    -- The post-construction view's node list, rewritten each frame at a node depth before the draw reads it.
    scratch("svo_debug_draw_node_list", sizes.debug_draw_node_list)
    return buffers
end

-- Bindings of the named shader resources to the buffers of the same names.
local function bind_named(builder: Builder, names: {string}): {string:graph.Resource}
    local result: {string:graph.Resource} = {}
    for _, name in ipairs(names) do
        local buffer = builder.buffers[name]
        if buffer == nil then
            error("voxel_svo: no buffer is named '" .. name .. "'.")
        end
        result[name] = buffer
    end
    return result
end

-- The shader resources of the passes, by role.
local CONSTRUCTION = { "svo_deferred_list_ping", "svo_info", "svo_leaf_buffer", "svo_node_buffer" }
local CLEAR = { "svo_deferred_list_ping", "svo_deferred_list_pong", "svo_info", "svo_node_buffer" }
local DEFERRED_CONSTRUCTION = { "svo_deferred_list_ping", "svo_deferred_list_pong", "svo_info", "svo_leaf_buffer", "svo_node_buffer" }
local PUBLICATION = { "svo_info", "svo_node_buffer" }
local OCCUPANCY = { "svo_info", "svo_leaf_buffer", "svo_node_buffer" }
local PRE_DEFERRED_CAPTURE = { "svo_deferred_list_ping", "svo_info", "svo_pre_deferred_debug_draw_indirect_buffer",
    "svo_pre_deferred_debug_info_buffer", "svo_pre_deferred_debug_list_buffer" }
local DEBUG_DRAW_SETUP = { "svo_debug_draw_indirect_buffer", "svo_debug_draw_node_list", "svo_debug_draw_settings", "svo_info",
    "svo_node_buffer" }
local DEBUG_DRAW_FIND_NODES = { "svo_debug_draw_indirect_buffer", "svo_debug_draw_node_list", "svo_debug_draw_settings",
    "svo_info", "svo_leaf_buffer", "svo_node_buffer" }
local TREE_DRAW = { "gbuffer_camera_info", "svo_debug_draw_node_list", "svo_debug_draw_settings", "svo_info", "svo_leaf_buffer",
    "svo_node_buffer", "svogi_occupancy_buffer" }
local PRE_DEFERRED_DRAW = { "gbuffer_camera_info", "svo_leaf_buffer", "svo_node_buffer", "svo_pre_deferred_debug_info_buffer",
    "svo_pre_deferred_debug_list_buffer" }

-- The compute pipelines, registered from the shaders of their keys when a task names them.
local function make_compute_pipelines(parameters: wbe.SVORecipeParams): {string:graph.Resource}
    local prefix = parameters.shader_prefix
    local dynamic_prefix = parameters.dynamic_shader_prefix
    return {
        reset = graph.shader_compute_pipeline(prefix .. "_reset_shader"),
        clear_dynamic = graph.shader_compute_pipeline(dynamic_prefix .. "_clear_shader"),
        static_ping = graph.shader_compute_pipeline(prefix .. "_build_deferred_propagation_pong_write_shader"),
        static_pong = graph.shader_compute_pipeline(prefix .. "_build_deferred_propagation_ping_write_shader"),
        dynamic_ping = graph.shader_compute_pipeline(dynamic_prefix .. "_build_deferred_propagation_pong_write_shader"),
        dynamic_pong = graph.shader_compute_pipeline(dynamic_prefix .. "_build_deferred_propagation_ping_write_shader"),
        static_publication = graph.shader_compute_pipeline("svo_2_update_states_shader"),
        dynamic_publication = graph.shader_compute_pipeline("svo_2_dynamic_update_states_shader"),
        reset_ping = graph.shader_compute_pipeline("svo_deferred_count_reset_pong_shader"),
        reset_pong = graph.shader_compute_pipeline("svo_deferred_count_reset_ping_shader"),
        capture_pre_deferred_debug = graph.shader_compute_pipeline("svo_pre_deferred_debug_capture_shader"),
        setup_debug_draw = graph.shader_compute_pipeline("svo_debug_draw_setup_shader"),
        find_debug_draw_nodes = graph.shader_compute_pipeline("svo_debug_draw_find_nodes_shader"),
        occupancy_injection = graph.shader_compute_pipeline("svogi_occupancy_injection"),
        occupancy_propagation = graph.shader_compute_pipeline("svogi_occupancy_propagation"),
        radiance_injection = graph.shader_compute_pipeline("svogi_radiance_injection"),
    }
end

-- The graphics pipelines. The constructions rasterize without attachments; the tree draws draw the cubes' edges, or their
-- faces in the solid styles, onto the target.
local function make_graphics_pipelines(parameters: wbe.SVORecipeParams, target: voxel.Target): {string:graph.Resource}
    local function construction_pipeline(key: string): graph.Resource
        return graph.graphics_pipeline(key, { pipeline = key })
    end
    local function overlay_pipeline(key: string, topology: wbe.PrimitiveTopologyVK): graph.Resource
        return graph.graphics_pipeline(key, {
            pipeline = key,
            color_formats = { target.color_format },
            depth_format = target.depth_format,
            primitive_topology = topology,
        })
    end
    return {
        static_construction = construction_pipeline(parameters.shader_prefix .. "_build_static_mesh_pipeline"),
        dynamic_construction = construction_pipeline(parameters.dynamic_shader_prefix .. "_build_static_mesh_pipeline"),
        -- The tree draw's branches, in the order the application selects them: the VoxelDebugDrawStyle, plus two in the
        -- occupancy view.
        tree_draw_wireframe = overlay_pipeline("svo_debug_draw_pipeline", "LINE_LIST"),
        tree_draw_solid = overlay_pipeline("svo_debug_draw_solid_pipeline", "TRIANGLE_LIST"),
        tree_draw_wireframe_occupancy = overlay_pipeline("svo_debug_draw_occupancy_pipeline", "LINE_LIST"),
        tree_draw_solid_occupancy = overlay_pipeline("svo_debug_draw_solid_occupancy_pipeline", "TRIANGLE_LIST"),
        pre_deferred_debug_draw = overlay_pipeline("svo_pre_deferred_debug_draw_pipeline", "LINE_LIST"),
    }
end

-- A single-dispatch compute task on compute_1, bound to the named buffers.
local function compute(builder: Builder, task_name: string, pipeline: graph.Resource,
    group_count_x: integer, names: {string}): graph.Task
    local params: graph.RTParamsRunComputePipelineVK =
        { pipeline = pipeline, group_count_x = group_count_x, queue_family = "compute_1" }
    return graph.compute(params, { name = task_name, bindings = bind_named(builder, names) })
end

-- A construction pass rasterizing one part of a mesh into the SVO.
local function construction(builder: Builder, task_name: string, mesh: graph.Resource, pipeline: graph.Resource,
    mesh_part: wbe.MeshPartVK, transform: graph.Resource, load_existing: boolean): graph.Task
    local load_op: wbe.AttachmentLoadOpVK = load_existing and "load" or "clear"
    local params: graph.RTParamsGraphicsDrawMeshVK = {
        mesh = mesh,
        pipeline = { default = pipeline },
        parts = { mesh_part },
        transform = transform,
        render_extent = builder.extent,
        color_load_op = load_op,
        depth_load_op = load_op,
    }
    return graph.draw_mesh(params, { name = task_name, bindings = bind_named(builder, CONSTRUCTION) })
end

-- The deferred construction after rasterization. Rasterization allocates depth one's children; every propagation round
-- publishes one further level, and an extra retry consumes leaf occupancy and drains the remaining deferred entries.
local function deferred_construction(builder: Builder, task_name: string, is_dynamic: boolean): graph.Task
    local parameters = builder.parameters
    local pipelines = builder.compute_pipelines
    local ping = is_dynamic and pipelines.dynamic_ping or pipelines.static_ping
    local pong = is_dynamic and pipelines.dynamic_pong or pipelines.static_pong
    if builder.pass_published then
        local publication = is_dynamic and pipelines.dynamic_publication or pipelines.static_publication
        local publication_group_count =
            is_dynamic and parameters.dynamic_publication_group_count or parameters.static_publication_group_count
        local dispatches: {graph.RTParamsComputeDispatchVK} = {}
        for iteration = 0, parameters.max_depth - 1 do
            local write_pong = iteration % 2 == 0
            dispatches[#dispatches + 1] = {
                pipeline = write_pong and pipelines.reset_ping or pipelines.reset_pong,
            }
            dispatches[#dispatches + 1] = { pipeline = write_pong and ping or pong, group_count_x = parameters.deferred_dispatch_group_count }
            dispatches[#dispatches + 1] = { pipeline = publication, group_count_x = publication_group_count }
        end
        local params: graph.RTParamsRunComputePipelinesSequentialVK = { dispatches = dispatches, queue_family = "compute_1" }
        return graph.compute_sequential(params, { name = task_name, bindings = bind_named(builder, DEFERRED_CONSTRUCTION) })
    end
    -- A single raster pass can leave contenders at any depth; allow one retry per tree level.
    local params: graph.RTParamsPingPongVK = {
        ping_pipeline = ping,
        pong_pipeline = pong,
        iterations = parameters.max_depth,
        group_count_x = parameters.deferred_dispatch_group_count,
        ping_prefix_pipeline = pipelines.reset_ping,
        pong_prefix_pipeline = pipelines.reset_pong,
        queue_family = "compute_1",
    }
    return graph.ping_pong(params, { name = task_name, bindings = bind_named(builder, DEFERRED_CONSTRUCTION) })
end

-- An indexed indirect overlay drawn on the target's color and depth while its toggle is enabled.
local function overlay(builder: Builder, task_name: string, pipeline: graph.Resource, indirect_buffer: graph.Resource,
    names: {string}, toggle: wbe.RendRecipeToggleVK): graph.Task
    local params: graph.RTParamsRunGraphicsPipelineIndirectVK = {
        pipeline = pipeline,
        index_buffer = builder.buffers.svo_debug_draw_index_buffer,
        indirect_buffer = indirect_buffer,
        color_load_op = "load",
        depth_load_op = "load",
    }
    local task = graph.draw_indirect(params, {
        name = task_name,
        bindings = bind_named(builder, names),
        toggle = toggle,
    })
    task:color_target(0, builder.color)
    task:depth_target(builder.depth)
    return task
end

-- The frame-begin barrier and the static scene, built once into the shared buffers unless the application rebuilds it
-- every frame (no trigger).
local function add_static_construction(builder: Builder)
    local parameters = builder.parameters
    local tasks = builder.tasks
    local static_build = parameters.static_build
    -- The frame-begin barrier orders this frame's SVO mutation after the previous frame's consumers.
    local barrier_params: graph.RTParamsFullMemoryBarrierVK = { resource = builder.buffers.svo_info }
    tasks.frame_begin = graph.full_memory_barrier(barrier_params, { name = "svo_frame_begin" })
    tasks.reset = compute(builder, "reset_svo", builder.compute_pipelines.reset, parameters.reset_group_count, CLEAR)
    tasks.reset.trigger = static_build
    local pipeline = builder.graphics_pipelines.static_construction
    tasks.build = construction(builder, "run_build_svo", builder.static_mesh, pipeline, "opaque", nil, false)
    tasks.build.trigger = static_build
    tasks.build_double_sided =
        construction(builder, "run_build_svo_double_sided", builder.static_mesh, pipeline, "double_sided", nil, false)
    tasks.build_double_sided.trigger = static_build
    tasks.static_publication = {}
    if builder.pass_published then
        local update_states = compute(builder, "update_svo_states", builder.compute_pipelines.static_publication,
            parameters.static_publication_group_count, PUBLICATION)
        update_states.trigger = static_build
        tasks.static_publication = { update_states }
    end
    tasks.static_deferred = deferred_construction(builder, "static_svo_deferred", false)
    tasks.static_deferred.trigger = static_build
end

-- The dynamic meshes, rebuilt every frame over the static scene, and the pre-deferred capture of the debug view.
local function add_dynamic_construction(builder: Builder)
    local parameters = builder.parameters
    local tasks = builder.tasks
    tasks.clear_dynamic = compute(builder, "clear_dynamic_svo", builder.compute_pipelines.clear_dynamic,
        parameters.dynamic_clear_group_count, CLEAR)
    tasks.dynamic = {}
    for index, id in ipairs({ "dynamic_svo", "dynamic_svo_second" }) do
        tasks.dynamic[index] = construction(builder, id, builder.dynamic_mesh, builder.graphics_pipelines.dynamic_construction,
            "opaque", builder.target.dynamic_mesh_transforms[index], true)
    end
    tasks.dynamic_publication = {}
    if builder.pass_published then
        tasks.dynamic_publication = { compute(builder, "publish_dynamic_svo", builder.compute_pipelines.dynamic_publication,
            parameters.dynamic_publication_group_count, PUBLICATION) }
    end
    tasks.capture_pre_deferred_debug = compute(builder, "capture_svo_pre_deferred_debug",
        builder.compute_pipelines.capture_pre_deferred_debug, parameters.deferred_dispatch_group_count, PRE_DEFERRED_CAPTURE)
    tasks.capture_pre_deferred_debug.toggle = builder.target.debug_view_toggles.pre_deferred_propagation
    tasks.dynamic_deferred = deferred_construction(builder, "run_build_svo_deferred", true)
end

-- SVOGI injection into the completed tree. The occupancy injection writes the leaves and their parents; propagation then
-- writes one level further up per dispatch, from the parents of the leaves' parents to the root's sectors. The radiance
-- injection stages the reflective shadow maps' flux after the radiance index reset.
local function add_svogi_injection(builder: Builder)
    local parameters = builder.parameters
    local buffers = builder.buffers
    local pipelines = builder.compute_pipelines
    local tasks = builder.tasks
    local occupancy_injection_params: graph.RTParamsRunComputePipelineVK = {
        pipeline = pipelines.occupancy_injection,
        group_count_x = parameters.occupancy_injection_group_count_x,
        group_count_y = parameters.occupancy_injection_group_count_y,
        queue_family = "compute_1",
    }
    local occupancy_bindings = bind_named(builder, OCCUPANCY)
    occupancy_bindings.atlas = buffers.svogi_occupancy_buffer
    tasks.occupancy_injection = graph.compute(occupancy_injection_params, {
        name = "inject_svogi_occupancy",
        bindings = occupancy_bindings,
    })
    local propagation_dispatches: {graph.RTParamsComputeDispatchVK} = {}
    for child_depth = parameters.max_depth - 1, 2, -1 do
        propagation_dispatches[#propagation_dispatches + 1] = {
            pipeline = pipelines.occupancy_propagation,
            group_count_x = parameters.occupancy_propagation_group_count,
            push_constants = { child_depth = child_depth },
        }
    end
    local propagation_params: graph.RTParamsRunComputePipelinesSequentialVK =
        { dispatches = propagation_dispatches, queue_family = "compute_1" }
    tasks.occupancy_propagation = graph.compute_sequential(propagation_params, {
        name = "propagate_svogi_occupancy",
        bindings = occupancy_bindings,
    })
    -- The radiance index buffer is a counted buffer, whose element byte size shares the deferred lists' count element; the
    -- pong list's count reset clears it.
    local reset_radiance_index_params: graph.RTParamsRunComputePipelineVK =
        { pipeline = pipelines.reset_ping, queue_family = "compute_1" }
    tasks.reset_radiance_index = graph.compute(reset_radiance_index_params, {
        name = "reset_svogi_radiance_index",
        bindings = { svo_deferred_list_pong = buffers.svogi_radiance_index_buffer },
    })
    local radiance_bindings = bind_named(builder, { "spot_light_infos", "svo_info", "svo_leaf_buffer", "svo_node_buffer" })
    radiance_bindings.index_buffer = buffers.svogi_radiance_index_buffer
    radiance_bindings.staged_buffer = buffers.svogi_radiance_staged_buffer
    -- The slots and the shadow sampler belong to the enclosing graph, so the subgraph reaches them through ports.
    radiance_bindings.shadow_sampler = graph.port("shadow_sampler")
    for slot in pairs(builder.target.rsm_slots) do
        radiance_bindings[slot] = graph.port(slot)
    end
    tasks.radiance_injection = graph.compute({
        pipeline = pipelines.radiance_injection,
        group_count_x = parameters.radiance_injection_group_count,
        group_count_y = parameters.radiance_injection_group_count,
        queue_family = "compute_1",
    }, { name = "inject_svogi_radiance", bindings = radiance_bindings })
    -- TODO(2): Sort the (key, index) pairs in svogi_radiance_index_buffer by key with sort.radix_sort_key_value_task()
    -- (render_graph_utils/sort.tl; pixel_sort.tl is an example). The counted buffer's count drives the indirect dispatches.
    local radiance_sort_buffers = sort.radix_sort_buffers({
        name = "svogi_radiance_sort",
        max_element_count = (parameters.buffer_sizes.radiance_index),
        key_type = "uint2",
    })
    buffers.svogi_radiance_index_buffer = radiance_sort_buffers.keys
    tasks.radiance_sort = sort.radix_sort_key_value_task(radiance_sort_buffers, {
        name = "SVOGI Radiance sort",
        queue_family = "GRAPHICS",
    })
    -- TODO(4): After the sort, add the reduction tasks:
    --   a. A compute pass that gathers svogi_radiance_staged_buffer through the sorted indices and sets each entry's
    --      `break` to 1 where its key differs from the previous key (and on the first entry).
    --   b. A segmented inclusive scan with SegmentedPrefix<float3x3, Add<float3x3>> (needs TODO(3)).
    --   c. A compute pass that writes each segment's last element (the next key differs, or it is the last entry) into
    --      the reduced radiance buffer at key / 27, subvoxel key % 27. Divide by the segment length for an average.
    -- TODO(5): Then propagate the reduced radiance up the tree, one depth per dispatch, mirroring
    -- tasks.occupancy_propagation above (a graph.compute_sequential over child_depth = max_depth - 1 down to 2).
end

-- The debug views: the post-construction tree, drawn by the post-SVO-build and occupancy views, and the pre-deferred
-- propagation view.
-- TODO(9): Add a radiance debug view next to the occupancy one (svo_debug_draw_*_occupancy_pipeline in svo/svo.toml and
-- the occupancy branches of the debug-draw multiplexer below), coloring nodes by their reduced or propagated radiance,
-- to check TODO(4) and TODO(5) before cone tracing depends on them.
local function add_debug_views(builder: Builder)
    local buffers = builder.buffers
    local pipelines = builder.graphics_pipelines
    local toggles = builder.target.debug_view_toggles
    local tasks = builder.tasks
    tasks.setup_debug_draw = compute(builder, "setup_svo_debug_draw", builder.compute_pipelines.setup_debug_draw, 1,
        DEBUG_DRAW_SETUP)
    -- The post-construction view's nodes at the depth svo_debug_draw_settings selects; the setup writes the root's sectors.
    tasks.find_debug_draw_nodes = compute(builder, "find_svo_debug_draw_nodes", builder.compute_pipelines.find_debug_draw_nodes,
        builder.parameters.debug_draw_node_search_group_count, DEBUG_DRAW_FIND_NODES)
    tasks.find_debug_draw_nodes.toggle = toggles.tree_view
    -- One multiplexer of every style and coloring, whose branches share the indirect command and the targets. The solid
    -- styles index the cube's faces.
    local tree_draw_branches: {graph.Task} = {}
    for _, branch in ipairs({
        { pipeline = pipelines.tree_draw_wireframe, index_buffer = buffers.svo_debug_draw_index_buffer },
        { pipeline = pipelines.tree_draw_solid, index_buffer = buffers.svo_debug_draw_solid_index_buffer },
        { pipeline = pipelines.tree_draw_wireframe_occupancy, index_buffer = buffers.svo_debug_draw_index_buffer },
        { pipeline = pipelines.tree_draw_solid_occupancy, index_buffer = buffers.svo_debug_draw_solid_index_buffer },
    }) do
        local params: graph.RTParamsRunGraphicsPipelineIndirectVK = {
            pipeline = branch.pipeline,
            index_buffer = branch.index_buffer,
            indirect_buffer = buffers.svo_debug_draw_indirect_buffer,
            color_load_op = "load",
            depth_load_op = "load",
        }
        tree_draw_branches[#tree_draw_branches + 1] = graph.draw_indirect(params, { name = "tree_draw_" .. #tree_draw_branches })
            :color_target(0, builder.color):depth_target(builder.depth)
    end
    -- The occupancy branches also read the occupancy.
    tasks.debug_draw = graph.multiplex({
        name = "run_svo_debug_draw",
        branches = tree_draw_branches,
        select = toggles.tree_draw,
        bindings = bind_named(builder, TREE_DRAW),
        toggle = toggles.tree_view,
    })
    tasks.pre_deferred_debug_draw = overlay(builder, "run_svo_pre_deferred_debug_draw", pipelines.pre_deferred_debug_draw,
        buffers.svo_pre_deferred_debug_draw_indirect_buffer, PRE_DEFERRED_DRAW, toggles.pre_deferred_propagation)
end

-- The tasks of every part, concatenated in order.
local function sequence(...: {graph.Task}): {graph.Task}
    local result: {graph.Task} = {}
    for _, part in ipairs({ ... }) do
        for _, task in ipairs(part) do
            result[#result + 1] = task
        end
    end
    return result
end

-- Every task using a buffer, in execution order; the publication passes of the pass-published implementation take their
-- place in the chains of the buffers they publish. The injection reads the completed tree; it reaches the graphics queue
-- through the overlays that follow it.
local function connect_tasks(builder: Builder)
    local buffers = builder.buffers
    local tasks = builder.tasks
    -- TODO(6): Append the sort, reduction, and radiance propagation tasks from TODO(2), TODO(4), and TODO(5) to
    -- `injection`, chain the new buffers through them (as svogi_occupancy_buffer is chained below), and list the tasks in
    -- the subgraph's task sequence in voxel_svo.subgraph().
    local injection = { tasks.occupancy_injection, tasks.occupancy_propagation, tasks.radiance_injection }
    -- The static rasterization, then the static deferred construction and the dynamic rasterization, which the publication
    -- passes follow.
    local static_rasterization = { tasks.reset, tasks.build, tasks.build_double_sided }
    local deferred_and_dynamic = { tasks.static_deferred, tasks.clear_dynamic, tasks.dynamic[1], tasks.dynamic[2] }
    graph.chain(buffers.svo_info, { tasks.frame_begin, tasks.reset })
    graph.chain(buffers.svo_info, sequence(static_rasterization, tasks.static_publication, deferred_and_dynamic,
        tasks.dynamic_publication, { tasks.capture_pre_deferred_debug, tasks.dynamic_deferred }, injection,
        { tasks.setup_debug_draw, tasks.find_debug_draw_nodes, tasks.debug_draw }))
    graph.chain(buffers.svo_node_buffer, sequence(static_rasterization, tasks.static_publication, deferred_and_dynamic,
        tasks.dynamic_publication, { tasks.dynamic_deferred }, injection,
        { tasks.setup_debug_draw, tasks.find_debug_draw_nodes, tasks.debug_draw, tasks.pre_deferred_debug_draw }))
    graph.chain(buffers.svo_leaf_buffer, sequence({
        tasks.build,
        tasks.build_double_sided,
        tasks.static_deferred,
        tasks.dynamic[1],
        tasks.dynamic[2],
        tasks.dynamic_deferred,
    }, injection, { tasks.find_debug_draw_nodes, tasks.debug_draw, tasks.pre_deferred_debug_draw }))
    graph.chain(buffers.svo_deferred_list_ping, {
        tasks.reset,
        tasks.build,
        tasks.build_double_sided,
        tasks.static_deferred,
        tasks.clear_dynamic,
        tasks.dynamic[1],
        tasks.dynamic[2],
        tasks.capture_pre_deferred_debug,
        tasks.dynamic_deferred,
    })
    graph.chain(buffers.svo_deferred_list_pong, { tasks.reset, tasks.static_deferred, tasks.clear_dynamic, tasks.dynamic_deferred })
    graph.chain({ buffers.svo_pre_deferred_debug_info_buffer, buffers.svo_pre_deferred_debug_list_buffer,
        buffers.svo_pre_deferred_debug_draw_indirect_buffer }, { tasks.capture_pre_deferred_debug, tasks.pre_deferred_debug_draw })
    graph.chain({ buffers.svo_debug_draw_indirect_buffer, buffers.svo_debug_draw_node_list },
        { tasks.setup_debug_draw, tasks.find_debug_draw_nodes, tasks.debug_draw })
    -- The occupancy view reads the propagated occupancy.
    graph.chain(buffers.svogi_occupancy_buffer, { tasks.occupancy_injection, tasks.occupancy_propagation, tasks.debug_draw })
    graph.chain(buffers.svogi_radiance_index_buffer, { tasks.reset_radiance_index, tasks.radiance_injection })
    -- The overlays draw in this order.
    graph.chain({ builder.color, builder.depth }, { tasks.debug_draw, tasks.pre_deferred_debug_draw })
end

-- The SVO as a subgraph task named name, whose overlays draw on its `color` and `depth` ports, bound to the target's.
function voxel_svo.subgraph(name: string, parameters: wbe.SVORecipeParams, target: voxel.Target): graph.Task
    local pass_published = is_pass_published(parameters)
    local buffers = create_buffers(parameters)
    local color = graph.port("color")
    local depth = graph.port("depth")
    local static_mesh = graph.mesh("static_mesh")
    local dynamic_mesh = graph.mesh("dynamic_mesh")
    local builder: Builder = {
        parameters = parameters,
        target = target,
        pass_published = pass_published,
        buffers = buffers,
        color = color,
        depth = depth,
        static_mesh = static_mesh,
        dynamic_mesh = dynamic_mesh,
        compute_pipelines = make_compute_pipelines(parameters),
        graphics_pipelines = make_graphics_pipelines(parameters, target),
        extent = { width = parameters.extent, height = parameters.extent },
        tasks = {},
    }
    add_static_construction(builder)
    add_dynamic_construction(builder)
    add_svogi_injection(builder)
    add_debug_views(builder)
    connect_tasks(builder)
    local ports: {string:graph.Resource} = {
        color = target.color_target,
        depth = target.depth_target,
        shadow_sampler = target.shadow_sampler,
    }
    for slot, resource in pairs(target.rsm_slots) do
        ports[slot] = resource
    end
    local tasks = builder.tasks
    return graph.subgraph({
        name = name,
        tasks = sequence({ tasks.frame_begin, tasks.reset, tasks.build, tasks.build_double_sided }, tasks.static_publication,
            { tasks.static_deferred, tasks.clear_dynamic }, tasks.dynamic, tasks.dynamic_publication, {
                tasks.capture_pre_deferred_debug,
                tasks.dynamic_deferred,
                tasks.occupancy_injection,
                tasks.occupancy_propagation,
                tasks.reset_radiance_index,
                tasks.radiance_injection,
                tasks.setup_debug_draw,
                tasks.find_debug_draw_nodes,
                tasks.debug_draw,
                tasks.pre_deferred_debug_draw,
            }),
        ports = ports,
    })
end

return voxel_svo
