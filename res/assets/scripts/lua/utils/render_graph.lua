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

local exports = {}

---A graph-owned storage buffer.
---@param p_id string
---@param p_size integer The size in bytes.
---@return wbe.RendRecipeResourceVK
function exports.storage_buffer(p_id, p_size)
    return { id = p_id, buffer = { usage = "buffer_storage", size = p_size } }
end

---A graph-owned 2D image the tasks read and write.
---@param p_id string
---@param p_format wbe.ImageFormatVK
---@param p_width integer
---@param p_height integer
---@param p_additional_usage? wbe.ImageUsageFlagVK[]
---@return wbe.RendRecipeResourceVK
function exports.storage_image(p_id, p_format, p_width, p_height, p_additional_usage)
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

---A default view of a graph-owned image.
---@param p_id string
---@param p_image string The id of the image.
---@return wbe.RendRecipeResourceVK
function exports.image_view(p_id, p_image)
    return { id = p_id, image_view = { image = p_image } }
end

---@class wbe.utils.TaskList
---@field tasks wbe.RecipeTask[] The tasks, in the order they were added.
---@field add fun(p_task: wbe.RecipeTask): wbe.RecipeTask Add a task and return it.
---@field get fun(p_id: string): wbe.RecipeTask Get an added task; errors when no task has the id.
---@field chain fun(p_resource: string, p_task_ids: string[]) Connect each task to the next on p_resource.

---A list of recipe tasks that can connect its tasks into resource chains.
---@return wbe.utils.TaskList
function exports.task_list()
    local tasks = {}
    local task_by_id = {}
    local list = { tasks = tasks }
    function list.add(p_task)
        if task_by_id[p_task.id] ~= nil then
            error("task_list: the task id '" .. tostring(p_task.id) .. "' is added twice.", 2)
        end
        tasks[#tasks + 1] = p_task
        task_by_id[p_task.id] = p_task
        return p_task
    end
    function list.get(p_id)
        local task = task_by_id[p_id]
        if task == nil then
            error("task_list: no task has the id '" .. tostring(p_id) .. "'.", 2)
        end
        return task
    end
    -- Order the tasks that use p_resource, in execution order.
    function list.chain(p_resource, p_task_ids)
        for i = 2, #p_task_ids do
            local task = list.get(p_task_ids[i - 1])
            task.connect = task.connect or {}
            task.connect[#task.connect + 1] = { to = p_task_ids[i], resource = p_resource }
        end
    end
    return list
end

return exports
