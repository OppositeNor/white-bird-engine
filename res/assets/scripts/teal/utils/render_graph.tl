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

-- Helpers for writing render graph recipes (see wbe.RendGraphRecipeVK).

local type wbe = require("wbe")

local record render_graph
    -- A list of recipe tasks that can connect its tasks into resource chains.
    record TaskList
        -- The tasks, in the order they were added.
        tasks: {wbe.RendRecipeTaskVK}
        -- Add a task and return it.
        add: function(p_task: wbe.RendRecipeTaskVK): wbe.RendRecipeTaskVK
        -- Get an added task; errors when no task has the id.
        get: function(p_id: string): wbe.RendRecipeTaskVK
        -- Connect each task to the next on p_resource.
        chain: function(p_resource: string, p_task_ids: {string})
    end
end

-- The canonical name of a reflected enum value the application passes. C++ serializes an enum as its enumerator name, and
-- the lowercase aliases recipes may also write name the same enumerator, so compare against the canonical upper-case name.
function render_graph.enum_name(p_value: string | integer): string
    if p_value is integer then
        error("enum_name: expected an enumerator name, got the integer " .. tostring(p_value) .. ".", 2)
    end
    return string.upper(p_value as string)
end

-- A graph-owned buffer of p_size bytes with the primary usage p_usage.
function render_graph.buffer(p_id: string, p_usage: wbe.RendResPrimaryUsageVK, p_size: integer): wbe.RendRecipeResourceVK
    return { id = p_id, buffer = { usage = p_usage, size = p_size } }
end

-- A graph-owned storage buffer of p_size bytes.
function render_graph.storage_buffer(p_id: string, p_size: integer): wbe.RendRecipeResourceVK
    return render_graph.buffer(p_id, "buffer_storage", p_size)
end

-- A graph-owned 2D image with the primary usage p_usage. Without p_extent, it keeps the default extent of
-- wbe.RendImageParamsVK.
function render_graph.image(p_id: string, p_usage: wbe.RendResPrimaryUsageVK, p_format: wbe.ImageFormatVK | integer,
    p_extent?: wbe.RendExtent2DParamsVK, p_additional_usage?: {wbe.ImageUsageFlagVK | integer},
    p_mip_levels?: integer): wbe.RendRecipeResourceVK
    return {
        id = p_id,
        image = {
            usage = p_usage,
            format = p_format,
            extent = p_extent,
            additional_usage = p_additional_usage,
            mip_levels = p_mip_levels,
        },
    }
end

-- A graph-owned 2D image the tasks read and write.
function render_graph.storage_image(p_id: string, p_format: wbe.ImageFormatVK | integer, p_width: integer, p_height: integer,
    p_additional_usage?: {wbe.ImageUsageFlagVK | integer}): wbe.RendRecipeResourceVK
    return render_graph.image(p_id, "image_read_write", p_format, { width = p_width, height = p_height }, p_additional_usage)
end

-- A view of the graph-owned image p_image, the default view unless p_view configures it.
function render_graph.image_view(p_id: string, p_image: string, p_view?: wbe.RendImageViewParamsVK): wbe.RendRecipeResourceVK
    return { id = p_id, image_view = { image = p_image, view = p_view } }
end

-- A sampler clamping every coordinate to the edge, otherwise configured by p_params.
function render_graph.clamp_to_edge_sampler(p_id: string, p_params?: wbe.RendSamplerParamsVK): wbe.RendRecipeResourceVK
    local params: wbe.RendSamplerParamsVK = p_params or {}
    params.address_mode_u = "clamp_to_edge"
    params.address_mode_v = "clamp_to_edge"
    params.address_mode_w = "clamp_to_edge"
    return { id = p_id, sampler = params }
end

-- A task that adds p_recipe, which declares its ports, as a subgraph under p_id; p_ports binds each port to a resource of
-- the enclosing recipe. Its tasks are prefixed `<p_id>.`; other tasks connect to it by p_id, and its trigger or toggle
-- gates all of its tasks.
function render_graph.subgraph(p_id: string, p_recipe: wbe.RendGraphRecipeVK, p_ports: {string:string}): wbe.RendRecipeTaskVK
    return { id = p_id, subgraph = p_recipe, ports = p_ports }
end

function render_graph.task_list(): render_graph.TaskList
    local tasks: {wbe.RendRecipeTaskVK} = {}
    local task_by_id: {string:wbe.RendRecipeTaskVK} = {}
    local list: render_graph.TaskList = { tasks = tasks }
    function list.add(p_task: wbe.RendRecipeTaskVK): wbe.RendRecipeTaskVK
        if task_by_id[p_task.id] ~= nil then
            error("task_list: the task id '" .. tostring(p_task.id) .. "' is added twice.", 2)
        end
        tasks[#tasks + 1] = p_task
        task_by_id[p_task.id] = p_task
        return p_task
    end
    function list.get(p_id: string): wbe.RendRecipeTaskVK
        local task = task_by_id[p_id]
        if task == nil then
            error("task_list: no task has the id '" .. tostring(p_id) .. "'.", 2)
        end
        return task
    end
    -- Order the tasks that use p_resource, in execution order.
    function list.chain(p_resource: string, p_task_ids: {string})
        for i = 2, #p_task_ids do
            local task = list.get(p_task_ids[i - 1])
            task.connect = task.connect or {}
            task.connect[#task.connect + 1] = { to = p_task_ids[i], resource = p_resource }
        end
    end
    return list
end

return render_graph
