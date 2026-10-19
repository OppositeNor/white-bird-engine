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

-- Mipmapped 3D-texture octree construction and debug views, for recipes that include it as a feature. The application
-- compiles the pipelines and owns the octree textures and buffers, which every graph imports. It also supplies the texture
-- clears and the construction passes as imported tasks, since they record image clears and report the full texture views
-- they write through the per-level views; see VoxelRenderFeatureOctree in
-- tests/executable/function/render/deferred_test/voxel_render_feature_octree.hh.

local type wbe = require("wbe")
local type voxel = require("render_graphs.features.voxel")
local type graph_utils = require("utils.render_graph")

local record voxel_octree
    record GroupCount
        x: integer
        y: integer
        z: integer
    end

    record Params
        -- The import names of the application's octree textures and buffers.
        imports: {string}
        -- The number of octree levels, which have one storage view each.
        level_count: integer
        -- The leaf gather's workgroups.
        leaf_gather_group_count: GroupCount
        -- The node gather's workgroups along each axis.
        node_gather_group_count: integer
        -- The leaf view's indirect draw records.
        leaf_draw_count: integer
    end
end

-- Add the octree's imports and tasks to a recipe under construction. Returns the overlay tasks, in drawing order; the
-- caller orders them on the color and depth targets.
function voxel_octree.add(p_graph: graph_utils.RecipeBuilder, p_params: voxel_octree.Params, p_target: voxel.Target): {string}
    for _, import in ipairs(p_params.imports) do
        p_graph.imports[#p_graph.imports + 1] = import
    end
    local tasks = p_graph.tasks
    local chain = tasks.chain
    local toggles = p_target.debug_view_toggles

    -- The frame-begin barrier orders this frame's mutation after the previous frame's consumers.
    local barrier_params: wbe.RTParamsFullMemoryBarrierVK = { resource = "octree_dynamic" }
    tasks.add({ id = "octree_frame_begin", type = "full_memory_barrier", params = barrier_params })
    -- Static scene: empty both textures, then occupy the rasterized leaves and their ancestors. Dynamic meshes: empty the
    -- dynamic texture, then rebuild it like the static scene.
    for _, id in ipairs({ "reset_octree", "run_build_octree", "run_build_octree_double_sided", "clear_dynamic_octree", "dynamic_octree",
        "dynamic_octree_second" }) do
        tasks.add({ id = id, import = true })
    end
    -- Debug views: the leaf gather serves both views, and the node gather only the node view.
    local leaf_groups = p_params.leaf_gather_group_count
    local leaf_gather_params: wbe.RTParamsRunComputePipelineVK = {
        pipeline = "octree_debug_gather_leaves_shader",
        group_count_x = leaf_groups.x,
        group_count_y = leaf_groups.y,
        group_count_z = leaf_groups.z,
        queue_family = "compute_1",
    }
    tasks.add({ id = "gather_octree_debug_leaves", type = "run_compute_pipeline", params = leaf_gather_params, toggle = toggles.enabled })
    local node_groups = p_params.node_gather_group_count
    local node_gather_params: wbe.RTParamsRunComputePipelineVK = {
        pipeline = "octree_debug_gather_nodes_shader",
        group_count_x = node_groups,
        group_count_y = node_groups,
        group_count_z = node_groups,
        queue_family = "compute_1",
    }
    tasks.add({
        id = "gather_octree_debug_nodes",
        type = "run_compute_pipeline",
        params = node_gather_params,
        toggle = toggles.pre_deferred_propagation,
    })
    local function overlay(p_id: string, p_pipeline: string, p_indirect_buffer: string, p_draw_count: integer,
        p_toggle: wbe.RendRecipeToggleVK)
        local params: wbe.RTParamsRunGraphicsPipelineIndirectVK = {
            pipeline = p_pipeline,
            index_buffer = "octree_debug_draw_index_buffer",
            indirect_buffer = p_indirect_buffer,
            draw_count = p_draw_count,
            color_load_op = "load",
            depth_load_op = "load",
        }
        tasks.add({
            id = p_id,
            type = "run_graphics_pipeline_indirect",
            params = params,
            aliases = { ["render_target_colors[0]"] = p_target.color_target, render_target_depth = p_target.depth_target },
            toggle = p_toggle,
        })
    end
    overlay("run_octree_debug_draw", "octree_debug_draw_pipeline", "octree_debug_draw_indirect_buffer", p_params.leaf_draw_count,
        toggles.enabled)
    overlay("run_octree_debug_node_draw", "octree_debug_node_draw_pipeline", "octree_debug_node_draw_indirect_buffer", 1,
        toggles.pre_deferred_propagation)

    -- The debug gathers read the full views after every pass writing the textures.
    chain("octree_dynamic", {
        "octree_frame_begin",
        "reset_octree",
        "clear_dynamic_octree",
        "dynamic_octree",
        "dynamic_octree_second",
        "gather_octree_debug_leaves",
        "gather_octree_debug_nodes",
    })
    chain("octree_static",
        { "reset_octree", "run_build_octree", "run_build_octree_double_sided", "gather_octree_debug_leaves", "gather_octree_debug_nodes" })
    -- The raster passes write every mip level through the per-level views.
    for level = 0, p_params.level_count - 1 do
        chain("octree_static_levels[" .. level .. "]", { "reset_octree", "run_build_octree", "run_build_octree_double_sided" })
        chain("octree_dynamic_levels[" .. level .. "]", { "reset_octree", "clear_dynamic_octree", "dynamic_octree", "dynamic_octree_second" })
    end
    chain("octree_debug_draw_indirect_buffer", { "gather_octree_debug_leaves", "run_octree_debug_draw" })
    chain("octree_debug_leaf_masks", { "gather_octree_debug_leaves", "run_octree_debug_draw" })
    -- The leaf gather empties the node view before the node gather fills it.
    chain("octree_debug_node_draw_indirect_buffer", { "gather_octree_debug_leaves", "gather_octree_debug_nodes", "run_octree_debug_node_draw" })
    chain("octree_debug_node_list", { "gather_octree_debug_leaves", "gather_octree_debug_nodes", "run_octree_debug_node_draw" })
    return { "run_octree_debug_draw", "run_octree_debug_node_draw" }
end

return voxel_octree
