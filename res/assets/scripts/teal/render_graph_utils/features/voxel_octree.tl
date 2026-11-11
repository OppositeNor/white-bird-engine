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

-- Mipmapped 3D-texture octree construction and debug views, for recipes that include it as a feature. The application owns
-- the octree textures and buffers, which every graph imports, and supplies the texture clears as imported tasks, since
-- they record image clears; see VoxelRenderFeatureOctree in
-- tests/executable/function/render/deferred_test/voxel_render_feature_octree.hh. The module registers its pipelines and
-- rasterizes the including recipe's "static_mesh" and "dynamic_mesh" into the textures; the construction shaders report the
-- full texture view they write through the per-level views.

local type wbe = require("wbe")
local type voxel = require("render_graph_utils.features.voxel")
local graph = require("render_graph_utils.render_graph_utils")

local voxel_octree = {}

-- The octree as a subgraph task named name, whose overlays draw on its `color` and `depth` ports, bound to the target's.
function voxel_octree.subgraph(name: string, parameters: wbe.OctreeRecipeParams, target: voxel.Target): graph.Task
    local resources: {string:graph.Resource} = {}
    local level_names: {string} = {}
    for level = 0, parameters.level_count - 1 do
        level_names[#level_names + 1] = "octree_static_levels[" .. level .. "]"
        level_names[#level_names + 1] = "octree_dynamic_levels[" .. level .. "]"
    end
    -- The application's textures and buffers, which the shaders name by their import ids, and the camera of the debug draws.
    for _, id in ipairs({ "octree_info", "octree_static", "octree_dynamic", "octree_debug_draw_index_buffer",
        "octree_debug_draw_indirect_buffer", "octree_debug_leaf_masks", "octree_debug_node_draw_indirect_buffer",
        "octree_debug_node_list", "gbuffer_camera_info" }) do
        resources[id] = graph.import(id)
    end
    for _, id in ipairs(level_names) do
        resources[id] = graph.import(id)
    end
    -- Bindings of the named shader resources to the imports of the same ids.
    local function imports_named(names: {string}): {string:graph.Resource}
        local result: {string:graph.Resource} = {}
        for _, name in ipairs(names) do
            result[name] = resources[name]
        end
        return result
    end
    local color = graph.port("color")
    local depth = graph.port("depth")
    local gather_leaves_pipeline = graph.shader_compute_pipeline("octree_debug_gather_leaves_shader")
    local gather_nodes_pipeline = graph.shader_compute_pipeline("octree_debug_gather_nodes_shader")
    -- The debug draws draw the cells' edges onto the target.
    local function debug_draw_pipeline_of(key: string): graph.Resource
        return graph.graphics_pipeline(key, {
            pipeline = key,
            color_formats = { target.color_format },
            depth_format = target.depth_format,
            primitive_topology = "LINE_LIST",
        })
    end
    -- The constructions rasterize without attachments.
    local build_pipeline = graph.graphics_pipeline("octree_build_static_mesh_pipeline",
        { pipeline = "octree_build_static_mesh_pipeline" })
    local dynamic_build_pipeline = graph.graphics_pipeline("octree_dynamic_build_static_mesh_pipeline",
        { pipeline = "octree_dynamic_build_static_mesh_pipeline" })
    local debug_draw_pipeline = debug_draw_pipeline_of("octree_debug_draw_pipeline")
    local debug_node_draw_pipeline = debug_draw_pipeline_of("octree_debug_node_draw_pipeline")
    local toggles = target.debug_view_toggles

    -- The frame-begin barrier orders this frame's mutation after the previous frame's consumers.
    local barrier_params: graph.RTParamsFullMemoryBarrierVK = { resource = resources.octree_dynamic }
    local frame_begin = graph.full_memory_barrier(barrier_params, { name = "octree_frame_begin" })
    -- Static scene: empty both textures, then occupy the rasterized leaves and their ancestors. Dynamic meshes: empty the
    -- dynamic texture, then rebuild it like the static scene.
    local extent: wbe.RendExtent2DParamsVK = { width = parameters.extent, height = parameters.extent }
    local function construction(task_name: string, mesh: graph.Resource, pipeline: graph.Resource,
        mesh_part: wbe.MeshPartVK, transform?: graph.Resource): graph.Task
        local params: graph.RTParamsGraphicsDrawMeshVK = {
            mesh = mesh,
            pipeline = { default = pipeline },
            parts = { mesh_part },
            transform = transform,
            render_extent = extent,
        }
        local texture = transform ~= nil and "octree_dynamic" or "octree_static"
        local names = { "octree_info", texture }
        for level = 0, parameters.level_count - 1 do
            names[#names + 1] = texture .. "_levels[" .. level .. "]"
        end
        return graph.draw_mesh(params, { name = task_name, bindings = imports_named(names) })
    end
    local static_mesh = graph.mesh("static_mesh")
    local dynamic_mesh = graph.mesh("dynamic_mesh")
    local reset = graph.import_task("reset_octree")
    -- The static construction records once unless the application rebuilds it every frame (no trigger); the application
    -- triggers the imported reset with the same event.
    local build = construction("run_build_octree", static_mesh, build_pipeline, "opaque")
    build.trigger = parameters.static_build
    local build_double_sided = construction("run_build_octree_double_sided", static_mesh, build_pipeline, "double_sided")
    build_double_sided.trigger = parameters.static_build
    local clear_dynamic = graph.import_task("clear_dynamic_octree")
    local dynamic = construction("dynamic_octree", dynamic_mesh, dynamic_build_pipeline, "opaque",
        target.dynamic_mesh_transforms[1])
    local dynamic_second = construction("dynamic_octree_second", dynamic_mesh, dynamic_build_pipeline, "opaque",
        target.dynamic_mesh_transforms[2])
    -- Debug views: the leaf gather serves both views, and the node gather only the node view.
    local leaf_groups = parameters.leaf_gather_group_count
    local leaf_gather_params: graph.RTParamsRunComputePipelineVK = {
        pipeline = gather_leaves_pipeline,
        group_count_x = leaf_groups.x,
        group_count_y = leaf_groups.y,
        group_count_z = leaf_groups.z,
        queue_family = "compute_1",
    }
    local gather_debug_leaves = graph.compute(leaf_gather_params, {
        name = "gather_octree_debug_leaves",
        bindings = imports_named({ "octree_info", "octree_static", "octree_dynamic", "octree_debug_draw_indirect_buffer",
            "octree_debug_leaf_masks", "octree_debug_node_draw_indirect_buffer", "octree_debug_node_list" }),
        toggle = toggles.enabled,
    })
    local node_groups = parameters.node_gather_group_count
    local node_gather_params: graph.RTParamsRunComputePipelineVK = {
        pipeline = gather_nodes_pipeline,
        group_count_x = node_groups,
        group_count_y = node_groups,
        group_count_z = node_groups,
        queue_family = "compute_1",
    }
    local gather_debug_nodes = graph.compute(node_gather_params, {
        name = "gather_octree_debug_nodes",
        bindings = imports_named({ "octree_info", "octree_static", "octree_dynamic", "octree_debug_node_draw_indirect_buffer",
            "octree_debug_node_list" }),
        toggle = toggles.pre_deferred_propagation,
    })
    local function overlay(task_name: string, pipeline: graph.Resource, indirect_buffer: graph.Resource, draw_count: integer,
        list: string, toggle: wbe.RendRecipeToggleVK): graph.Task
        local params: graph.RTParamsRunGraphicsPipelineIndirectVK = {
            pipeline = pipeline,
            index_buffer = resources.octree_debug_draw_index_buffer,
            indirect_buffer = indirect_buffer,
            draw_count = draw_count,
            color_load_op = "load",
            depth_load_op = "load",
        }
        local task = graph.draw_indirect(params, {
            name = task_name,
            bindings = imports_named({ "octree_info", "gbuffer_camera_info", list }),
            toggle = toggle,
        })
        task:color_target(0, color)
        task:depth_target(depth)
        return task
    end
    local debug_draw = overlay("run_octree_debug_draw", debug_draw_pipeline, resources.octree_debug_draw_indirect_buffer, parameters.leaf_draw_count,
        "octree_debug_leaf_masks", toggles.enabled)
    local debug_node_draw = overlay("run_octree_debug_node_draw", debug_node_draw_pipeline, resources.octree_debug_node_draw_indirect_buffer, 1,
        "octree_debug_node_list", toggles.pre_deferred_propagation)

    -- The debug gathers read the full views after every pass writing the textures.
    graph.chain(resources.octree_dynamic, {
        frame_begin,
        reset,
        clear_dynamic,
        dynamic,
        dynamic_second,
        gather_debug_leaves,
        gather_debug_nodes,
    })
    graph.chain(resources.octree_static,
        { reset, build, build_double_sided, gather_debug_leaves, gather_debug_nodes })
    -- The raster passes write every mip level through the per-level views.
    for level = 0, parameters.level_count - 1 do
        graph.chain(resources["octree_static_levels[" .. level .. "]"], { reset, build, build_double_sided })
        graph.chain(resources["octree_dynamic_levels[" .. level .. "]"], { reset, clear_dynamic, dynamic, dynamic_second })
    end
    gather_debug_leaves:connect(debug_draw, { resources.octree_debug_draw_indirect_buffer, resources.octree_debug_leaf_masks })
    -- The leaf gather empties the node view before the node gather fills it.
    graph.chain({ resources.octree_debug_node_draw_indirect_buffer, resources.octree_debug_node_list },
        { gather_debug_leaves, gather_debug_nodes, debug_node_draw })
    -- The overlays draw in this order.
    graph.chain({ color, depth }, { debug_draw, debug_node_draw })
    return graph.subgraph({
        name = name,
        tasks = { frame_begin, reset, build, build_double_sided, clear_dynamic, dynamic, dynamic_second, gather_debug_leaves,
            gather_debug_nodes, debug_draw, debug_node_draw },
        ports = { color = target.color_target, depth = target.depth_target },
    })
end

return voxel_octree
