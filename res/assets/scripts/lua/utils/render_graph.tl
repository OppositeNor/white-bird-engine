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

    -- A recipe under construction, which feature modules' add() functions add imports, resources, and tasks to.
    record RecipeBuilder
        imports: {string}
        resources: {wbe.RendRecipeResourceVK}
        tasks: TaskList
    end
end

-- A graph-owned storage buffer of p_size bytes.
function render_graph.storage_buffer(p_id: string, p_size: integer): wbe.RendRecipeResourceVK
    return { id = p_id, buffer = { usage = "buffer_storage", size = p_size } }
end

-- A graph-owned 2D image the tasks read and write.
function render_graph.storage_image(p_id: string, p_format: wbe.ImageFormatVK | integer, p_width: integer, p_height: integer,
    p_additional_usage?: {wbe.ImageUsageFlagVK | integer}): wbe.RendRecipeResourceVK
    return {
        id = p_id,
        image = {
            usage = "image_read_write",
            format = p_format,
            extent = { width = p_width, height = p_height },
            additional_usage = p_additional_usage,
        },
    }
end

-- A default view of the graph-owned image p_image.
function render_graph.image_view(p_id: string, p_image: string): wbe.RendRecipeResourceVK
    return { id = p_id, image_view = { image = p_image } }
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
