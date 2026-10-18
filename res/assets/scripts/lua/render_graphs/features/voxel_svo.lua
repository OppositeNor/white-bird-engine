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

local graph_utils = require("utils.render_graph")

local exports = {}

---@class wbe.features.VoxelSVOBufferSizes
---@field dynamic_nodes integer svo_dynamic_nodes.
---@field deferred_list integer Each of svo_deferred_list_ping and svo_deferred_list_pong.
---@field pre_deferred_debug_info integer svo_pre_deferred_debug_info_buffer.
---@field pre_deferred_debug_list integer svo_pre_deferred_debug_list_buffer.

---@class wbe.features.VoxelSVOParams
---@field variant "atomic"|"pass_published"
---@field shader_prefix string Resource-key prefix of the static construction pipelines.
---@field dynamic_shader_prefix string Resource-key prefix of the dynamic update pipelines.
---@field imports string[] The import names of the application's SVO buffers.
---@field buffer_sizes wbe.features.VoxelSVOBufferSizes The byte sizes of the scratch buffers the module declares.
---@field extent integer The rasterized resolution of the construction passes.
---@field max_depth integer The tree depth, which is the number of deferred propagation rounds.
---@field reset_group_count integer
---@field deferred_dispatch_group_count integer
---@field dynamic_clear_group_count integer
---@field static_publication_group_count integer
---@field dynamic_publication_group_count integer
---@field static_build wbe.RendRecipeTriggerVK? The trigger of the static construction tasks, which record once; nil to rebuild every frame.

---@class wbe.features.VoxelTarget
---@field dynamic_mesh_transform_ids string[] The transforms of the two dynamic mesh entities.
---@field debug_view_toggles table<string, wbe.RendRecipeToggleVK> The voxel debug view toggles by mode group.
---@field color_target string The color target of the overlays.
---@field depth_target string The depth target the overlays test against.

---Add the SVO's imports and tasks to a recipe under construction.
---@param p_graph { imports: string[], resources: wbe.RendRecipeResourceVK[], tasks: wbe.utils.TaskList }
---@param p_params wbe.features.VoxelSVOParams
---@param p_target wbe.features.VoxelTarget
---@return string[] overlays The overlay tasks, in drawing order; the caller orders them on the color and depth targets.
function exports.add(p_graph, p_params, p_target)
    local pass_published = p_params.variant == "pass_published"
    if not pass_published and p_params.variant ~= "atomic" then
        error("voxel_svo: unknown variant '" .. tostring(p_params.variant) .. "'.")
    end
    for _, import in ipairs(p_params.imports) do
        p_graph.imports[#p_graph.imports + 1] = import
    end
    -- Scratch buffers: each frame's dynamic clear (and the static reset) empties the dynamic nodes' range and the
    -- deferred lists before the construction fills them, and the pre-deferred capture rewrites its copies before the
    -- debug draw reads them.
    local resources = p_graph.resources
    local function add_buffer(p_id, p_size)
        resources[#resources + 1] = graph_utils.storage_buffer(p_id, p_size)
    end
    local sizes = p_params.buffer_sizes
    add_buffer("svo_dynamic_nodes", sizes.dynamic_nodes)
    add_buffer("svo_deferred_list_ping", sizes.deferred_list)
    add_buffer("svo_deferred_list_pong", sizes.deferred_list)
    add_buffer("svo_pre_deferred_debug_info_buffer", sizes.pre_deferred_debug_info)
    add_buffer("svo_pre_deferred_debug_list_buffer", sizes.pre_deferred_debug_list)
    local tasks = p_graph.tasks
    local chain = tasks.chain
    local prefix = p_params.shader_prefix
    local dynamic_prefix = p_params.dynamic_shader_prefix
    local toggles = p_target.debug_view_toggles
    local static_build = p_params.static_build
    local extent = { width = p_params.extent, height = p_params.extent }

    local function compute(p_id, p_pipeline, p_group_count_x, p_extra)
        local task = {
            id = p_id,
            type = "run_compute_pipeline",
            params = { pipeline = p_pipeline, group_count_x = p_group_count_x, queue_family = "compute_1" },
        }
        for key, value in pairs(p_extra or {}) do
            task[key] = value
        end
        return tasks.add(task)
    end
    local function construction(p_id, p_mesh, p_pipeline, p_part, p_transform, p_load, p_extra)
        local load_op = p_load and "load" or "clear"
        local task = {
            id = p_id,
            type = "graphics_draw_mesh",
            params = {
                mesh = p_mesh,
                pipeline = p_pipeline,
                parts = { p_part },
                transform = p_transform,
                render_extent = extent,
                color_load_op = load_op,
                depth_load_op = load_op,
            },
        }
        for key, value in pairs(p_extra or {}) do
            task[key] = value
        end
        return tasks.add(task)
    end
    -- Rasterization allocates depth one's children; every propagation round publishes one further level, and an extra
    -- retry consumes leaf occupancy and drains the remaining deferred entries.
    local function deferred_construction(p_id, p_dynamic, p_extra)
        local stage_prefix = p_dynamic and dynamic_prefix or prefix
        local ping = stage_prefix .. "_build_deferred_propagation_pong_write_shader"
        local pong = stage_prefix .. "_build_deferred_propagation_ping_write_shader"
        local task
        if pass_published then
            local publication = p_dynamic and "svo_2_dynamic_update_states_shader" or "svo_2_update_states_shader"
            local publication_group_count =
                p_dynamic and p_params.dynamic_publication_group_count or p_params.static_publication_group_count
            local dispatches = {}
            for iteration = 0, p_params.max_depth - 1 do
                local write_pong = iteration % 2 == 0
                dispatches[#dispatches + 1] = {
                    pipeline = write_pong and "svo_deferred_count_reset_pong_shader" or "svo_deferred_count_reset_ping_shader",
                }
                dispatches[#dispatches + 1] = { pipeline = write_pong and ping or pong, group_count_x = p_params.deferred_dispatch_group_count }
                dispatches[#dispatches + 1] = { pipeline = publication, group_count_x = publication_group_count }
            end
            task = { id = p_id, type = "run_compute_pipelines_sequential", params = { dispatches = dispatches, queue_family = "compute_1" } }
        else
            -- A single raster pass can leave contenders at any depth; allow one retry per tree level.
            task = {
                id = p_id,
                type = "ping_pong",
                params = {
                    ping_pipeline = ping,
                    pong_pipeline = pong,
                    iterations = p_params.max_depth,
                    group_count_x = p_params.deferred_dispatch_group_count,
                    ping_prefix_pipeline = "svo_deferred_count_reset_pong_shader",
                    pong_prefix_pipeline = "svo_deferred_count_reset_ping_shader",
                    queue_family = "compute_1",
                },
            }
        end
        for key, value in pairs(p_extra or {}) do
            task[key] = value
        end
        return tasks.add(task)
    end
    local function overlay(p_id, p_pipeline, p_indirect_buffer, p_toggle)
        tasks.add({
            id = p_id,
            type = "run_graphics_pipeline_indirect",
            params = {
                pipeline = p_pipeline,
                index_buffer = "svo_debug_draw_index_buffer",
                indirect_buffer = p_indirect_buffer,
                color_load_op = "load",
                depth_load_op = "load",
            },
            aliases = { ["render_target_colors[0]"] = p_target.color_target, render_target_depth = p_target.depth_target },
            toggle = p_toggle,
        })
    end
    -- The static scene is built once into the shared buffers, unless the application rebuilds it every frame.
    local static_once = static_build ~= nil and { trigger = static_build } or nil

    -- The frame-begin barrier orders this frame's SVO mutation after the previous frame's consumers.
    tasks.add({ id = "svo_frame_begin", type = "full_memory_barrier", params = { resource = "svo_info" } })
    -- Static scene.
    compute("reset_svo", prefix .. "_reset_shader", p_params.reset_group_count, static_once)
    construction("run_build_svo", "static_mesh", prefix .. "_build_static_mesh_pipeline", "opaque", nil, false, static_once)
    construction("run_build_svo_double_sided", "static_mesh", prefix .. "_build_static_mesh_pipeline", "double_sided", nil, false,
        static_once)
    if pass_published then
        compute("update_svo_states", "svo_2_update_states_shader", p_params.static_publication_group_count, static_once)
    end
    deferred_construction("static_svo_deferred", false, static_once)
    -- Dynamic meshes.
    compute("clear_dynamic_svo", dynamic_prefix .. "_clear_shader", p_params.dynamic_clear_group_count)
    for index, id in ipairs({ "dynamic_svo", "dynamic_svo_second" }) do
        construction(id, "dynamic_mesh", dynamic_prefix .. "_build_static_mesh_pipeline", "opaque",
            p_target.dynamic_mesh_transform_ids[index], true)
    end
    if pass_published then
        compute("publish_dynamic_svo", "svo_2_dynamic_update_states_shader", p_params.dynamic_publication_group_count)
    end
    compute("capture_svo_pre_deferred_debug", "svo_pre_deferred_debug_capture_shader", p_params.deferred_dispatch_group_count,
        { toggle = toggles.pre_deferred_propagation })
    deferred_construction("run_build_svo_deferred", true)
    compute("setup_svo_debug_draw", "svo_debug_draw_setup_shader", 1)
    -- Debug views.
    overlay("run_svo_debug_draw", "svo_debug_draw_pipeline", "svo_debug_draw_indirect_buffer", toggles.post_svo_build)
    overlay("run_svo_pre_deferred_debug_draw", "svo_pre_deferred_debug_draw_pipeline", "svo_pre_deferred_debug_draw_indirect_buffer",
        toggles.pre_deferred_propagation)

    -- Every task using a buffer, in execution order; the publication passes of the pass-published implementation take
    -- their place in the chains of the buffers they publish.
    local static_publication = pass_published and { "update_svo_states" } or {}
    local dynamic_publication = pass_published and { "publish_dynamic_svo" } or {}
    local function sequence(...)
        local result = {}
        for _, part in ipairs({ ... }) do
            for _, id in ipairs(part) do
                result[#result + 1] = id
            end
        end
        return result
    end
    chain("svo_info", { "svo_frame_begin", "reset_svo" })
    chain("svo_info", sequence({ "reset_svo", "run_build_svo", "run_build_svo_double_sided" }, static_publication,
        { "static_svo_deferred", "clear_dynamic_svo", "dynamic_svo", "dynamic_svo_second" }, dynamic_publication,
        { "capture_svo_pre_deferred_debug", "run_build_svo_deferred", "setup_svo_debug_draw", "run_svo_debug_draw" }))
    chain("svo_node_buffer", sequence({ "reset_svo", "run_build_svo", "run_build_svo_double_sided" }, static_publication,
        { "static_svo_deferred", "clear_dynamic_svo", "dynamic_svo", "dynamic_svo_second" }, dynamic_publication,
        { "run_build_svo_deferred", "run_svo_debug_draw", "run_svo_pre_deferred_debug_draw" }))
    for _, buffer in ipairs({ "svo_dynamic_nodes", "svo_dynamic_leaves" }) do
        chain(buffer, sequence({ "clear_dynamic_svo", "dynamic_svo", "dynamic_svo_second" }, dynamic_publication,
            { "run_build_svo_deferred", "run_svo_debug_draw", "run_svo_pre_deferred_debug_draw" }))
    end
    chain("svo_leaf_buffer", {
        "run_build_svo",
        "run_build_svo_double_sided",
        "static_svo_deferred",
        "dynamic_svo",
        "dynamic_svo_second",
        "run_build_svo_deferred",
        "run_svo_debug_draw",
        "run_svo_pre_deferred_debug_draw",
    })
    chain("svo_deferred_list_ping", {
        "reset_svo",
        "run_build_svo",
        "run_build_svo_double_sided",
        "static_svo_deferred",
        "clear_dynamic_svo",
        "dynamic_svo",
        "dynamic_svo_second",
        "capture_svo_pre_deferred_debug",
        "run_build_svo_deferred",
    })
    chain("svo_deferred_list_pong", { "reset_svo", "static_svo_deferred", "clear_dynamic_svo", "run_build_svo_deferred" })
    for _, buffer in ipairs({ "svo_pre_deferred_debug_info_buffer", "svo_pre_deferred_debug_list_buffer",
        "svo_pre_deferred_debug_draw_indirect_buffer" }) do
        chain(buffer, { "capture_svo_pre_deferred_debug", "run_svo_pre_deferred_debug_draw" })
    end
    chain("svo_debug_draw_indirect_buffer", { "setup_svo_debug_draw", "run_svo_debug_draw" })
    return { "run_svo_debug_draw", "run_svo_pre_deferred_debug_draw" }
end

return exports
