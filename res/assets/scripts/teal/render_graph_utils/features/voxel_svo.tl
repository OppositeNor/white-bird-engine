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
-- the SVO buffers with initial contents or state both deferred graphs share, which every graph imports, and the module
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

-- The subgraph's tasks, by role. The publication lists are empty in the atomic implementation.
local record Tasks
    frame_begin: render_graph.Task
    reset: render_graph.Task
    build: render_graph.Task
    build_double_sided: render_graph.Task
    static_publication: {render_graph.Task}
    static_deferred: render_graph.Task
    clear_dynamic: render_graph.Task
    dynamic: {render_graph.Task}
    dynamic_publication: {render_graph.Task}
    capture_pre_deferred_debug: render_graph.Task
    dynamic_deferred: render_graph.Task
    occupancy_injection: render_graph.Task
    occupancy_propagation: render_graph.Task
    reset_radiance_index: render_graph.Task
    radiance_injection: render_graph.Task
    setup_debug_draw: render_graph.Task
    find_debug_draw_nodes: render_graph.Task
    debug_draw: render_graph.Task
    pre_deferred_debug_draw: render_graph.Task
end

-- The subgraph under construction, shared by the build steps.
local record Builder
    graph: render_graph.Graph
    parameters: wbe.SVORecipeParams
    target: voxel.Target
    -- Whether this is the pass-published implementation, rather than the atomic one.
    pass_published: boolean
    buffers: {string:render_graph.Resource}
    color: render_graph.Resource
    depth: render_graph.Resource
    static_mesh: render_graph.Resource
    dynamic_mesh: render_graph.Resource
    compute_pipelines: {string:render_graph.Resource}
    graphics_pipelines: {string:render_graph.Resource}
    -- The render area of the construction rasterization.
    extent: wbe.RendExtent2DParamsVK
    tasks: Tasks
end

-- Whether parameters select the pass-published implementation; fails on an unknown variant.
local function is_pass_published(parameters: wbe.SVORecipeParams): boolean
    local variant = render_graph.enum_name(parameters.variant)
    local pass_published = variant == "PASS_PUBLISHED"
    if not pass_published and variant ~= "ATOMIC" then
        error("voxel_svo: unknown variant '" .. variant .. "'.")
    end
    return pass_published
end

-- The application's buffers, and the scratch buffers. Each frame's dynamic clear (and the static reset) empties the
-- dynamic nodes' range and the deferred lists before the construction fills them, and the pre-deferred capture rewrites
-- its copies before the debug draw reads them.
local function create_buffers(parameters: wbe.SVORecipeParams): {string:render_graph.Resource}
    local buffers: {string:render_graph.Resource} = {}
    for _, id in ipairs(parameters.imports) do
        buffers[id] = render_graph.import_resource(id)
    end
    local sizes = parameters.buffer_sizes
    buffers.svo_deferred_list_ping = render_graph.storage_buffer("svo_deferred_list_ping", sizes.deferred_list)
    buffers.svo_deferred_list_pong = render_graph.storage_buffer("svo_deferred_list_pong", sizes.deferred_list)
    buffers.svo_pre_deferred_debug_info_buffer =
        render_graph.storage_buffer("svo_pre_deferred_debug_info_buffer", sizes.pre_deferred_debug_info)
    buffers.svo_pre_deferred_debug_list_buffer =
        render_graph.storage_buffer("svo_pre_deferred_debug_list_buffer", sizes.pre_deferred_debug_list)
    -- The SVOGI injection rewrites these every frame: the occupancy of every occupied leaf and every node with children,
    -- and the radiance entries after the count reset.
    buffers.svogi_occupancy_buffer = render_graph.storage_buffer("svogi_occupancy_buffer", sizes.occupancy)
    buffers.svogi_radiance_index_buffer = render_graph.storage_buffer("svogi_radiance_index_buffer", sizes.radiance_index)
    buffers.svogi_radiance_staged_buffer = render_graph.storage_buffer("svogi_radiance_staged_buffer", sizes.radiance_staged)
    -- The post-construction view's node list, rewritten each frame at a node depth before the draw reads it.
    buffers.svo_debug_draw_node_list = render_graph.storage_buffer("svo_debug_draw_node_list", sizes.debug_draw_node_list)
    return buffers
end

-- The compute pipelines, registered from the shaders of their keys when a task names them.
local function make_compute_pipelines(parameters: wbe.SVORecipeParams): {string:render_graph.Resource}
    local prefix = parameters.shader_prefix
    local dynamic_prefix = parameters.dynamic_shader_prefix
    return {
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
end

-- The graphics pipelines. The constructions rasterize without attachments; the tree draws draw the cubes' edges, or their
-- faces in the solid styles, onto the target.
local function make_graphics_pipelines(parameters: wbe.SVORecipeParams, target: voxel.Target): {string:render_graph.Resource}
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

-- A single-dispatch compute task on compute_1.
local function compute(builder: Builder, task_id: string, pipeline: render_graph.Resource,
    group_count_x: integer): render_graph.Task
    local params: render_graph.RTParamsRunComputePipelineVK =
        { pipeline = pipeline, group_count_x = group_count_x, queue_family = "compute_1" }
    return builder.graph:compute(task_id, params)
end

-- A construction pass rasterizing one part of a mesh into the SVO.
local function construction(builder: Builder, task_id: string, mesh: render_graph.Resource, pipeline: render_graph.Resource,
    mesh_part: wbe.MeshPartVK, transform: render_graph.Resource, load_existing: boolean): render_graph.Task
    local load_op: wbe.AttachmentLoadOpVK = load_existing and "load" or "clear"
    local params: render_graph.RTParamsGraphicsDrawMeshVK = {
        mesh = mesh,
        pipeline = { default = pipeline },
        parts = { mesh_part },
        transform = transform,
        render_extent = builder.extent,
        color_load_op = load_op,
        depth_load_op = load_op,
    }
    return builder.graph:draw_mesh(task_id, params)
end

-- The deferred construction after rasterization. Rasterization allocates depth one's children; every propagation round
-- publishes one further level, and an extra retry consumes leaf occupancy and drains the remaining deferred entries.
local function deferred_construction(builder: Builder, task_id: string, is_dynamic: boolean): render_graph.Task
    local parameters = builder.parameters
    local pipelines = builder.compute_pipelines
    local ping = is_dynamic and pipelines.dynamic_ping or pipelines.static_ping
    local pong = is_dynamic and pipelines.dynamic_pong or pipelines.static_pong
    if builder.pass_published then
        local publication = is_dynamic and pipelines.dynamic_publication or pipelines.static_publication
        local publication_group_count =
            is_dynamic and parameters.dynamic_publication_group_count or parameters.static_publication_group_count
        local dispatches: {render_graph.RTParamsComputeDispatchVK} = {}
        for iteration = 0, parameters.max_depth - 1 do
            local write_pong = iteration % 2 == 0
            dispatches[#dispatches + 1] = {
                pipeline = write_pong and pipelines.reset_ping or pipelines.reset_pong,
            }
            dispatches[#dispatches + 1] = { pipeline = write_pong and ping or pong, group_count_x = parameters.deferred_dispatch_group_count }
            dispatches[#dispatches + 1] = { pipeline = publication, group_count_x = publication_group_count }
        end
        local params: render_graph.RTParamsRunComputePipelinesSequentialVK = { dispatches = dispatches, queue_family = "compute_1" }
        return builder.graph:compute_sequential(task_id, params)
    end
    -- A single raster pass can leave contenders at any depth; allow one retry per tree level.
    local params: render_graph.RTParamsPingPongVK = {
        ping_pipeline = ping,
        pong_pipeline = pong,
        iterations = parameters.max_depth,
        group_count_x = parameters.deferred_dispatch_group_count,
        ping_prefix_pipeline = pipelines.reset_ping,
        pong_prefix_pipeline = pipelines.reset_pong,
        queue_family = "compute_1",
    }
    return builder.graph:ping_pong(task_id, params)
end

-- An indexed indirect overlay drawn on the target's color and depth while its toggle is enabled.
local function overlay(builder: Builder, task_id: string, pipeline: render_graph.Resource, indirect_buffer: render_graph.Resource,
    toggle: wbe.RendRecipeToggleVK): render_graph.Task
    local params: render_graph.RTParamsRunGraphicsPipelineIndirectVK = {
        pipeline = pipeline,
        index_buffer = builder.buffers.svo_debug_draw_index_buffer,
        indirect_buffer = indirect_buffer,
        color_load_op = "load",
        depth_load_op = "load",
    }
    local task = builder.graph:draw_indirect(task_id, params, {
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
    local barrier_params: render_graph.RTParamsFullMemoryBarrierVK = { resource = builder.buffers.svo_info }
    tasks.frame_begin = builder.graph:full_memory_barrier("svo_frame_begin", barrier_params)
    tasks.reset = compute(builder, "reset_svo", builder.compute_pipelines.reset, parameters.reset_group_count)
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
            parameters.static_publication_group_count)
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
        parameters.dynamic_clear_group_count)
    tasks.dynamic = {}
    for index, id in ipairs({ "dynamic_svo", "dynamic_svo_second" }) do
        tasks.dynamic[index] = construction(builder, id, builder.dynamic_mesh, builder.graphics_pipelines.dynamic_construction,
            "opaque", render_graph.reference(builder.target.dynamic_mesh_transform_ids[index]), true)
    end
    tasks.dynamic_publication = {}
    if builder.pass_published then
        tasks.dynamic_publication = { compute(builder, "publish_dynamic_svo", builder.compute_pipelines.dynamic_publication,
            parameters.dynamic_publication_group_count) }
    end
    tasks.capture_pre_deferred_debug = compute(builder, "capture_svo_pre_deferred_debug",
        builder.compute_pipelines.capture_pre_deferred_debug, parameters.deferred_dispatch_group_count)
    tasks.capture_pre_deferred_debug.toggle = builder.target.debug_view_toggles.pre_deferred_propagation
    tasks.dynamic_deferred = deferred_construction(builder, "run_build_svo_deferred", true)
end

-- SVOGI injection into the completed tree. The occupancy injection writes the leaves and their parents; propagation then
-- writes one level further up per dispatch, from the parents of the leaves' parents to the root's sectors. The radiance
-- injection stages the reflective shadow maps' flux after the radiance index reset.
local function add_svogi_injection(builder: Builder)
    local graph = builder.graph
    local parameters = builder.parameters
    local buffers = builder.buffers
    local pipelines = builder.compute_pipelines
    local tasks = builder.tasks
    local occupancy_injection_params: render_graph.RTParamsRunComputePipelineVK = {
        pipeline = pipelines.occupancy_injection,
        group_count_x = parameters.occupancy_injection_group_count_x,
        group_count_y = parameters.occupancy_injection_group_count_y,
        queue_family = "compute_1",
    }
    tasks.occupancy_injection = graph:compute("inject_svogi_occupancy", occupancy_injection_params, {
        bindings = { atlas = buffers.svogi_occupancy_buffer },
    })
    local propagation_dispatches: {render_graph.RTParamsComputeDispatchVK} = {}
    for child_depth = parameters.max_depth - 1, 2, -1 do
        propagation_dispatches[#propagation_dispatches + 1] = {
            pipeline = pipelines.occupancy_propagation,
            group_count_x = parameters.occupancy_propagation_group_count,
            push_constants = { child_depth = child_depth },
        }
    end
    local propagation_params: render_graph.RTParamsRunComputePipelinesSequentialVK =
        { dispatches = propagation_dispatches, queue_family = "compute_1" }
    tasks.occupancy_propagation = graph:compute_sequential("propagate_svogi_occupancy", propagation_params, {
        bindings = { atlas = buffers.svogi_occupancy_buffer },
    })
    -- The radiance index buffer is a counted buffer, whose element byte size shares the deferred lists' count element; the
    -- pong list's count reset clears it.
    local reset_radiance_index_params: render_graph.RTParamsRunComputePipelineVK =
        { pipeline = pipelines.reset_ping, queue_family = "compute_1" }
    tasks.reset_radiance_index = graph:compute("reset_svogi_radiance_index", reset_radiance_index_params, {
        bindings = { svo_deferred_list_pong = buffers.svogi_radiance_index_buffer },
    })
    local radiance_bindings: {string:render_graph.Resource} = {
        index_buffer = buffers.svogi_radiance_index_buffer,
        staged_buffer = buffers.svogi_radiance_staged_buffer,
    }
    for slot, id in pairs(builder.target.rsm_slot_ids) do
        radiance_bindings[slot] = render_graph.reference(id)
    end
    local radiance_injection_params: render_graph.RTParamsRunComputePipelineVK = {
        pipeline = pipelines.radiance_injection,
        group_count_x = parameters.radiance_injection_group_count,
        group_count_y = parameters.radiance_injection_group_count,
        queue_family = "compute_1",
    }
    tasks.radiance_injection = graph:compute("inject_svogi_radiance", radiance_injection_params, { bindings = radiance_bindings })
end

-- The debug views: the post-construction tree, drawn by the post-SVO-build and occupancy views, and the pre-deferred
-- propagation view.
local function add_debug_views(builder: Builder)
    local buffers = builder.buffers
    local pipelines = builder.graphics_pipelines
    local toggles = builder.target.debug_view_toggles
    local tasks = builder.tasks
    tasks.setup_debug_draw = compute(builder, "setup_svo_debug_draw", builder.compute_pipelines.setup_debug_draw, 1)
    -- The post-construction view's nodes at the depth svo_debug_draw_settings selects; the setup writes the root's sectors.
    tasks.find_debug_draw_nodes = compute(builder, "find_svo_debug_draw_nodes", builder.compute_pipelines.find_debug_draw_nodes,
        builder.parameters.debug_draw_node_search_group_count)
    tasks.find_debug_draw_nodes.toggle = toggles.tree_view
    -- One multiplexer of every style and coloring, whose branches share the indirect command and the targets. The solid
    -- styles index the cube's faces.
    local branch_graph = render_graph.new()
    local tree_draw_branches: {render_graph.Task} = {}
    for _, branch in ipairs({
        { pipeline = pipelines.tree_draw_wireframe, index_buffer = buffers.svo_debug_draw_index_buffer },
        { pipeline = pipelines.tree_draw_solid, index_buffer = buffers.svo_debug_draw_solid_index_buffer },
        { pipeline = pipelines.tree_draw_wireframe_occupancy, index_buffer = buffers.svo_debug_draw_index_buffer },
        { pipeline = pipelines.tree_draw_solid_occupancy, index_buffer = buffers.svo_debug_draw_solid_index_buffer },
    }) do
        local params: render_graph.RTParamsRunGraphicsPipelineIndirectVK = {
            pipeline = branch.pipeline,
            index_buffer = branch.index_buffer,
            indirect_buffer = buffers.svo_debug_draw_indirect_buffer,
            color_load_op = "load",
            depth_load_op = "load",
        }
        tree_draw_branches[#tree_draw_branches + 1] = branch_graph:draw_indirect("tree_draw_" .. #tree_draw_branches, params)
            :color_target(0, builder.color):depth_target(builder.depth)
    end
    tasks.debug_draw = builder.graph:multiplex("run_svo_debug_draw", tree_draw_branches, {
        select = toggles.tree_draw,
        toggle = toggles.tree_view,
    })
    tasks.pre_deferred_debug_draw = overlay(builder, "run_svo_pre_deferred_debug_draw", pipelines.pre_deferred_debug_draw,
        buffers.svo_pre_deferred_debug_draw_indirect_buffer, toggles.pre_deferred_propagation)
end

-- The tasks of every part, concatenated in order.
local function sequence(...: {render_graph.Task}): {render_graph.Task}
    local result: {render_graph.Task} = {}
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
    local graph = builder.graph
    local buffers = builder.buffers
    local tasks = builder.tasks
    local injection = { tasks.occupancy_injection, tasks.occupancy_propagation, tasks.radiance_injection }
    -- The static rasterization, then the static deferred construction and the dynamic rasterization, which the publication
    -- passes follow.
    local static_rasterization = { tasks.reset, tasks.build, tasks.build_double_sided }
    local deferred_and_dynamic = { tasks.static_deferred, tasks.clear_dynamic, tasks.dynamic[1], tasks.dynamic[2] }
    graph:chain(buffers.svo_info, { tasks.frame_begin, tasks.reset })
    graph:chain(buffers.svo_info, sequence(static_rasterization, tasks.static_publication, deferred_and_dynamic,
        tasks.dynamic_publication, { tasks.capture_pre_deferred_debug, tasks.dynamic_deferred }, injection,
        { tasks.setup_debug_draw, tasks.find_debug_draw_nodes, tasks.debug_draw }))
    graph:chain(buffers.svo_node_buffer, sequence(static_rasterization, tasks.static_publication, deferred_and_dynamic,
        tasks.dynamic_publication, { tasks.dynamic_deferred }, injection,
        { tasks.setup_debug_draw, tasks.find_debug_draw_nodes, tasks.debug_draw, tasks.pre_deferred_debug_draw }))
    graph:chain(buffers.svo_leaf_buffer, sequence({
        tasks.build,
        tasks.build_double_sided,
        tasks.static_deferred,
        tasks.dynamic[1],
        tasks.dynamic[2],
        tasks.dynamic_deferred,
    }, injection, { tasks.find_debug_draw_nodes, tasks.debug_draw, tasks.pre_deferred_debug_draw }))
    graph:chain(buffers.svo_deferred_list_ping, {
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
    graph:chain(buffers.svo_deferred_list_pong, { tasks.reset, tasks.static_deferred, tasks.clear_dynamic, tasks.dynamic_deferred })
    graph:chain({ buffers.svo_pre_deferred_debug_info_buffer, buffers.svo_pre_deferred_debug_list_buffer,
        buffers.svo_pre_deferred_debug_draw_indirect_buffer }, { tasks.capture_pre_deferred_debug, tasks.pre_deferred_debug_draw })
    graph:chain({ buffers.svo_debug_draw_indirect_buffer, buffers.svo_debug_draw_node_list },
        { tasks.setup_debug_draw, tasks.find_debug_draw_nodes, tasks.debug_draw })
    -- The occupancy view reads the propagated occupancy.
    graph:chain(buffers.svogi_occupancy_buffer, { tasks.occupancy_injection, tasks.occupancy_propagation, tasks.debug_draw })
    graph:chain(buffers.svogi_radiance_index_buffer, { tasks.reset_radiance_index, tasks.radiance_injection })
    -- The overlays draw in this order.
    graph:chain({ builder.color, builder.depth }, { tasks.debug_draw, tasks.pre_deferred_debug_draw })
end

-- The SVO as a subgraph task with id identifier, whose overlays draw on its `color` and `depth` ports, bound to the target's.
function voxel_svo.subgraph(identifier: string, parameters: wbe.SVORecipeParams, target: voxel.Target): render_graph.Task
    local pass_published = is_pass_published(parameters)
    local graph = render_graph.new()
    local buffers = create_buffers(parameters)
    local color = render_graph.port("color")
    local depth = render_graph.port("depth")
    local static_mesh = render_graph.mesh("static_mesh")
    local dynamic_mesh = render_graph.mesh("dynamic_mesh")
    local builder: Builder = {
        graph = graph,
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
    return graph:as_subgraph(identifier, { color = target.color_target, depth = target.depth_target }, parameters.imports)
end

return voxel_svo
