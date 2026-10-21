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

-- Handle-based helpers for writing render graph recipes (see wbe.RendGraphRecipeVK).
-- Resource and task handles describe a graph; Graph:build() emits the plain recipe tables C++ consumes.

local type wbe = require("wbe")
local render_graph_tasks = require("utils.render_graph_tasks")

local record render_graph
    image_view: function(p_id: string, p_image: Resource, p_view?: wbe.RendImageViewParamsVK): Resource
    -- A graph-owned description, an external import, or a subgraph port. Image views retain their source handle.
    record Resource
        id: string
        kind: string
        description: wbe.RendRecipeResourceVK
        dependencies: {Resource}
        view: function(self: Resource, p_id?: string, p_view?: wbe.RendImageViewParamsVK): Resource
    end

    -- Resource-bearing task parameters mirror the reflected C++ descriptions, accepting handles in place of the resource
    -- IDs of their WBE_RECIPE_RESOURCE fields (see utils.render_graph_tasks).
    -- String IDs remain accepted for existing recipes and task-local names in sequential dispatches.
    record DrawMeshParams is render_graph_tasks.RTParamsGraphicsDrawMeshVK<Resource>
    end
    record DrawIndirectParams is render_graph_tasks.RTParamsRunGraphicsPipelineIndirectVK<Resource>
    end
    record ComputeIndirectParams is render_graph_tasks.RTParamsRunComputePipelineIndirectVK<Resource>
    end
    record ComputeDispatchParams is render_graph_tasks.RTParamsComputeDispatchVK<Resource>
    end
    record ComputeSequentialParams is render_graph_tasks.RTParamsRunComputePipelinesSequentialVK<Resource>
    end
    record GenerateMipmapsParams is render_graph_tasks.RTParamsGenerateMipmapsVK<Resource>
    end
    record CPUAcquireImageParams is render_graph_tasks.RTParamsCPUInFlightAcquireVK<Resource>
    end
    record FullMemoryBarrierParams is render_graph_tasks.RTParamsFullMemoryBarrierVK<Resource>
    end

    record TaskOptions
        aliases: {string:Resource}
        uses: {Resource}
        trigger: wbe.RendRecipeTriggerVK
        toggle: wbe.RendRecipeToggleVK
    end

    record Edge
        to: Task
        resource: Resource
    end

    -- Resource handles bind task-local names without changing the explicit ordering of connect edges.
    record Task
        id: string
        ["type"]: string
        import: boolean
        params: any
        aliases: {string:Resource}
        edges: {Edge}
        graph: Graph
        alias: function(self: Task, p_local_name: string, p_resource: Resource): Task
        connect: function(self: Task, p_next_task: Task, p_resource: Resource): Task
        connect: function(self: Task, p_next_task: Task, p_resources: {Resource}): Task
        connect_all: function(self: Task, p_next_tasks: {Task}, p_resource: Resource): Task
        connect_all: function(self: Task, p_next_tasks: {Task}, p_resources: {Resource}): Task
        inputs_from: function(self: Task, p_producer: Task, p_bindings: {string:Resource}): Task
        color_target: function(self: Task, p_index: integer, p_resource: Resource): Task
        depth_target: function(self: Task, p_resource: Resource): Task
        trigger: wbe.RendRecipeTriggerVK
        toggle: wbe.RendRecipeToggleVK
        subgraph: wbe.RendGraphRecipeVK
        ports: {string:Resource}
        -- Retain resources named directly by typed task parameters or reflected same-name shader bindings.
        -- This collects resources and does not declare an access or add an ordering edge.
        uses: {Resource}
    end

    -- The typed task constructors, such as compute and draw_mesh, come from the render tasks annotated
    -- WBE_RENDER_TASK(<method>) (see utils.render_graph_tasks), which must not shadow the members declared here.
    record Graph is render_graph_tasks.Graph<Resource, Task, TaskOptions, Graph>
        tasks: {Task}
        add: function(self: Graph, p_task: Task): Task
        get: function(self: Graph, p_id: string): Task
        import_task: function(self: Graph, p_id: string): Task
        chain: function(self: Graph, p_resource: Resource, p_tasks: {Task})
        chain: function(self: Graph, p_resources: {Resource}, p_tasks: {Task})
        build: function(self: Graph, p_imports?: {string}, p_ports?: {string}): wbe.RendGraphRecipeVK
    end
end

local function validate_id(p_id: string)
    if type(p_id) ~= "string" or p_id == "" then
        error("render_graph: resource and task ids must be non-empty strings.", 3)
    end
end

local function sorted_keys<T>(p_map: {string:T}): {string}
    local keys: {string} = {}
    for key in pairs(p_map) do
        keys[#keys + 1] = key
    end
    table.sort(keys)
    return keys
end

local function validate_resource(p_resource: render_graph.Resource)
    if type(p_resource) ~= "table" then
        error("render_graph: expected a resource handle.", 3)
    end
    validate_id(p_resource.id)
    if type(p_resource.kind) ~= "string" or type(p_resource.dependencies) ~= "table" then
        error("render_graph: expected a resource handle.", 3)
    end
end

local function validate_list<T>(p_values: {T}, p_name: string, p_allow_empty: boolean)
    if type(p_values) ~= "table" then
        error("render_graph: expected a dense " .. p_name .. " list.", 3)
    end
    local count = 0
    for key in pairs(p_values as {any:any}) do
        if type(key) ~= "number" then
            error("render_graph: expected a dense " .. p_name .. " list.", 3)
        end
        local index = key as number
        if index < 1 or index % 1 ~= 0 then
            error("render_graph: expected a dense " .. p_name .. " list.", 3)
        end
        count = count + 1
    end
    for i = 1, count do
        if p_values[i] == nil then
            error("render_graph: expected a dense " .. p_name .. " list.", 3)
        end
    end
    if count == 0 and not p_allow_empty then
        error("render_graph: the " .. p_name .. " list must not be empty.", 3)
    end
end

-- Teal cannot discriminate a record/array union; the public methods expose typed overloads instead.
local function connection_resources(p_resources: any): {render_graph.Resource}
    if type(p_resources) ~= "table" then
        error("render_graph: expected a resource handle or a non-empty resource list.", 3)
    end
    if (p_resources as render_graph.Resource).id ~= nil then
        local resource = p_resources as render_graph.Resource
        validate_resource(resource)
        return { resource }
    end
    local resources = p_resources as {render_graph.Resource}
    validate_list(resources, "resource", false)
    for _, resource in ipairs(resources) do
        validate_resource(resource)
    end
    return resources
end

local function append_edge(p_producer: render_graph.Task, p_consumer: render_graph.Task,
    p_resource: render_graph.Resource)
    p_producer.edges = p_producer.edges or {}
    p_producer.edges[#p_producer.edges + 1] = { to = p_consumer, resource = p_resource }
end

local function attach_task(p_task: render_graph.Task,
    p_validate_task?: function(p_task: render_graph.Task, p_local_name?: string,
        p_resource?: render_graph.Resource)): render_graph.Task
    function p_task:alias(p_local_name: string, p_resource: render_graph.Resource): render_graph.Task
        if p_validate_task == nil then
            error("render_graph: add the task to a graph before assigning an alias.", 2)
        end
        validate_id(p_local_name)
        validate_resource(p_resource)
        p_validate_task(self, p_local_name, p_resource)
        self.aliases = self.aliases or {}
        if self.aliases[p_local_name] ~= nil then
            error("render_graph: task-local resource '" .. p_local_name .. "' is mapped more than once.", 2)
        end
        self.aliases[p_local_name] = p_resource
        return self
    end

    local function connect(self: render_graph.Task, p_next_task: render_graph.Task,
        p_resources: any): render_graph.Task
        if p_validate_task == nil then
            error("render_graph: add the task to a graph before connecting it.", 2)
        end
        p_validate_task(self)
        p_validate_task(p_next_task)
        self.graph:chain(connection_resources(p_resources), { self, p_next_task })
        return self
    end
    p_task.connect = connect

    local function connect_all(self: render_graph.Task, p_next_tasks: {render_graph.Task},
        p_resources: any): render_graph.Task
        if p_validate_task == nil then
            error("render_graph: add the task to a graph before connecting it.", 2)
        end
        p_validate_task(self)
        validate_list(p_next_tasks, "target task", false)
        for _, task in ipairs(p_next_tasks) do
            p_validate_task(task)
        end
        local resources = connection_resources(p_resources)
        for _, resource in ipairs(resources) do
            for _, task in ipairs(p_next_tasks) do
                append_edge(self, task, resource)
            end
        end
        return self
    end
    p_task.connect_all = connect_all

    function p_task:inputs_from(p_producer: render_graph.Task,
        p_bindings: {string:render_graph.Resource}): render_graph.Task
        if p_validate_task == nil then
            error("render_graph: add the task to a graph before assigning inputs.", 2)
        end
        p_validate_task(self)
        p_validate_task(p_producer)
        if type(p_bindings) ~= "table" or next(p_bindings) == nil then
            error("render_graph: input bindings must be a non-empty map of task-local names to resources.", 2)
        end
        for local_name, resource in pairs(p_bindings) do
            validate_id(local_name)
            validate_resource(resource)
            p_validate_task(self, local_name, resource)
            if self.aliases ~= nil and self.aliases[local_name] ~= nil then
                error("render_graph: task-local resource '" .. local_name .. "' is mapped more than once.", 2)
            end
        end
        self.aliases = self.aliases or {}
        local connected: {render_graph.Resource:boolean} = {}
        for _, local_name in ipairs(sorted_keys(p_bindings)) do
            local resource = p_bindings[local_name]
            self.aliases[local_name] = resource
            if not connected[resource] then
                append_edge(p_producer, self, resource)
                connected[resource] = true
            end
        end
        return self
    end

    function p_task:color_target(p_index: integer, p_resource: render_graph.Resource): render_graph.Task
        if type(p_index) ~= "number" or p_index < 0 or p_index % 1 ~= 0 then
            error("render_graph: color attachment indices must be non-negative integers.", 2)
        end
        return self:alias(string.format("render_target_colors[%d]", p_index), p_resource)
    end

    function p_task:depth_target(p_resource: render_graph.Resource): render_graph.Task
        return self:alias("render_target_depth", p_resource)
    end
    return p_task
end

local function attach_resource(p_resource: render_graph.Resource): render_graph.Resource
    local default_view: render_graph.Resource
    function p_resource:view(p_id?: string, p_view?: wbe.RendImageViewParamsVK): render_graph.Resource
        if self.kind ~= "image" and self.kind ~= "import" and self.kind ~= "reference" and self.kind ~= "port" then
            error("render_graph: image views require an image resource.", 2)
        end
        if p_id == nil then
            if p_view ~= nil then
                error("render_graph: a customized image view requires an explicit id.", 2)
            end
            if default_view == nil then
                default_view = render_graph.image_view(self.id .. ".view", self)
            end
            return default_view
        end
        return render_graph.image_view(p_id, self, p_view)
    end
    return p_resource
end

local function owned_resource(p_description: wbe.RendRecipeResourceVK, p_kind: string,
    p_dependencies?: {render_graph.Resource}): render_graph.Resource
    validate_id(p_description.id)
    return attach_resource({ id = p_description.id, kind = p_kind, description = p_description, dependencies = p_dependencies or {} })
end

-- The canonical name of a reflected enum value the application passes. C++ serializes an enum as its enumerator name,
-- and lowercase aliases recipes may also write name the same enumerator.
function render_graph.enum_name(p_value: string | integer): string
    if p_value is integer then
        error("enum_name: expected an enumerator name, got the integer " .. tostring(p_value) .. ".", 2)
    end
    return string.upper(p_value as string)
end

function render_graph.buffer(p_id: string, p_usage: wbe.RendResPrimaryUsageVK, p_size: integer): render_graph.Resource
    return owned_resource({ id = p_id, buffer = { usage = p_usage, size = p_size } }, "buffer")
end

function render_graph.storage_buffer(p_id: string, p_size: integer): render_graph.Resource
    return render_graph.buffer(p_id, "buffer_storage", p_size)
end

-- A graph-owned 2D image. Omitted extent keeps the default of wbe.RendImageParamsVK.
function render_graph.image(p_id: string, p_usage: wbe.RendResPrimaryUsageVK, p_format: wbe.ImageFormatVK | integer,
    p_extent?: wbe.RendExtent2DParamsVK, p_additional_usage?: {wbe.ImageUsageFlagVK | integer},
    p_mip_levels?: integer): render_graph.Resource
    return owned_resource({
        id = p_id,
        image = {
            usage = p_usage,
            format = p_format,
            extent = p_extent,
            additional_usage = p_additional_usage,
            mip_levels = p_mip_levels,
        },
    }, "image")
end

function render_graph.storage_image(p_id: string, p_format: wbe.ImageFormatVK | integer, p_width: integer, p_height: integer,
    p_additional_usage?: {wbe.ImageUsageFlagVK | integer}): render_graph.Resource
    return render_graph.image(p_id, "image_read_write", p_format, { width = p_width, height = p_height }, p_additional_usage)
end

-- A view keeps its source image reachable even when no task accesses the image directly.
function render_graph.image_view(p_id: string, p_image: render_graph.Resource,
    p_view?: wbe.RendImageViewParamsVK): render_graph.Resource
    if p_image.kind ~= "image" and p_image.kind ~= "import" and p_image.kind ~= "reference" and p_image.kind ~= "port" then
        error("render_graph: image views require an image resource.", 2)
    end
    return owned_resource({ id = p_id, image_view = { image = p_image.id, view = p_view } }, "image_view", { p_image })
end

function render_graph.clamp_to_edge_sampler(p_id: string, p_params?: wbe.RendSamplerParamsVK): render_graph.Resource
    local params: wbe.RendSamplerParamsVK = p_params or {}
    params.address_mode_u = "clamp_to_edge"
    params.address_mode_v = "clamp_to_edge"
    params.address_mode_w = "clamp_to_edge"
    return owned_resource({ id = p_id, sampler = params }, "sampler")
end

-- External resources are supplied by C++; a repeated import handle with the same id denotes the same import.
function render_graph.import_resource(p_id: string): render_graph.Resource
    validate_id(p_id)
    return attach_resource({ id = p_id, kind = "import", dependencies = {} })
end

-- Ports belong to the nested recipe and are bound by its enclosing subgraph task.
function render_graph.port(p_id: string): render_graph.Resource
    validate_id(p_id)
    return attach_resource({ id = p_id, kind = "port", dependencies = {} })
end

-- Reference an enclosing resource or a member of an imported composite without declaring another import.
-- C++ resolves and validates the id when it assembles the graph.
function render_graph.reference(p_id: string): render_graph.Resource
    validate_id(p_id)
    return attach_resource({ id = p_id, kind = "reference", dependencies = {} })
end

-- A dispatch uses task-local string aliases: these names are resolved inside the containing task's handle bindings.
function render_graph.indirect_dispatch(p_pipeline: string, p_indirect_buffer: string, p_offset: integer,
    p_aliases?: {string:string}): wbe.RTParamsComputeDispatchVK
    return { pipeline = p_pipeline, indirect_buffer = p_indirect_buffer, indirect_offset = p_offset, aliases = p_aliases }
end

function render_graph.subgraph(p_id: string, p_recipe: wbe.RendGraphRecipeVK,
    p_ports: {string:render_graph.Resource}): render_graph.Task
    return attach_task({ id = p_id, subgraph = p_recipe, ports = p_ports })
end

-- Snapshot plain task parameters after the adapters have replaced their documented resource fields.
local function copy_data(p_value: any): any
    if type(p_value) ~= "table" then
        return p_value
    end
    local result: {any:any} = {}
    for key, value in pairs(p_value as {any:any}) do
        result[key] = copy_data(value)
    end
    return result
end

local function copy_fields<T>(p_value: T): {string:any}
    local result: {string:any} = {}
    for key, value in pairs(p_value as {string:any}) do
        result[key] = value
    end
    return result
end

local function resource_id(p_value: render_graph.Resource | string, p_uses: {render_graph.Resource}): string
    if p_value == nil then
        return nil
    end
    if p_value is string then
        return p_value
    end
    validate_id(p_value.id)
    p_uses[#p_uses + 1] = p_value
    return p_value.id
end

function render_graph.graph(): render_graph.Graph
    local tasks: {render_graph.Task} = {}
    local task_by_id: {string:render_graph.Task} = {}
    local registered_ids: {render_graph.Task:string} = {}
    local parameter_ids: {render_graph.Task:{string:boolean}} = {}
    local registered_count = 0
    local graph: render_graph.Graph = { tasks = tasks }

    local function validate_parameter_alias(p_task: render_graph.Task, p_local_name: string,
        p_resource: render_graph.Resource)
        local ids = parameter_ids[p_task]
        if ids ~= nil and ids[p_local_name] and p_resource.id ~= p_local_name then
            error("render_graph: task parameter resource '" .. p_local_name .. "' conflicts with a task-local alias.", 3)
        end
    end

    local function validate_parameter_aliases(p_task: render_graph.Task)
        for local_name, resource in pairs(p_task.aliases or {}) do
            validate_parameter_alias(p_task, local_name, resource)
        end
    end

    local function validate_task(p_task: render_graph.Task, p_local_name?: string,
        p_resource?: render_graph.Resource)
        if type(p_task) ~= "table" then
            error("render_graph: expected a task handle belonging to this graph.", 3)
        end
        local id = registered_ids[p_task]
        if p_task.graph ~= graph or id == nil or task_by_id[id] ~= p_task then
            error("render_graph: task handle does not belong to this graph.", 3)
        end
        if p_task.id ~= id then
            error("render_graph: the registered task id '" .. id .. "' was changed.", 3)
        end
        validate_parameter_aliases(p_task)
        if p_local_name ~= nil then
            validate_parameter_alias(p_task, p_local_name, p_resource)
        end
    end

    function graph:add(p_task: render_graph.Task): render_graph.Task
        validate_id(p_task.id)
        if p_task.graph ~= nil and p_task.graph ~= self then
            error("render_graph: task '" .. p_task.id .. "' already belongs to another graph.", 2)
        end
        if task_by_id[p_task.id] ~= nil then
            error("render_graph: the task id '" .. p_task.id .. "' is added twice.", 2)
        end
        if registered_ids[p_task] ~= nil then
            error("render_graph: the registered task id was changed before adding it again.", 2)
        end
        if p_task.aliases ~= nil then
            local aliases: {string:render_graph.Resource} = {}
            for local_name, resource in pairs(p_task.aliases) do
                aliases[local_name] = resource
            end
            p_task.aliases = aliases
        end
        if p_task.edges ~= nil then
            local edges: {render_graph.Edge} = {}
            for _, edge in ipairs(p_task.edges) do
                edges[#edges + 1] = { to = edge.to, resource = edge.resource }
            end
            p_task.edges = edges
        end
        attach_task(p_task, validate_task)
        p_task.graph = self
        self.tasks[#self.tasks + 1] = p_task
        task_by_id[p_task.id] = p_task
        registered_ids[p_task] = p_task.id
        registered_count = registered_count + 1
        return p_task
    end

    local function make_task(p_id: string, p_type: string, p_params: any,
        p_options?: render_graph.TaskOptions, p_uses?: {render_graph.Resource}): render_graph.Task
        local options: render_graph.TaskOptions = p_options or {}
        local uses: {render_graph.Resource} = {}
        for _, resource in ipairs(options.uses or {}) do
            uses[#uses + 1] = resource
        end
        for _, resource in ipairs(p_uses or {}) do
            uses[#uses + 1] = resource
        end
        local task: render_graph.Task = {
            id = p_id,
            type = p_type,
            params = copy_data(p_params),
            aliases = options.aliases,
            uses = uses,
            trigger = copy_data(options.trigger) as wbe.RendRecipeTriggerVK,
            toggle = copy_data(options.toggle) as wbe.RendRecipeToggleVK,
        }
        local ids: {string:boolean} = {}
        for _, resource in ipairs(p_uses or {}) do
            ids[resource.id] = true
        end
        parameter_ids[task] = ids
        validate_parameter_aliases(task)
        return graph:add(task)
    end

    -- The typed constructors lower the resource handles of their params to IDs and retain the handles. A task-local
    -- handle also binds its graph ID as an identity alias, which cannot shadow an unrelated same-name shader binding.
    -- Every handle stays reachable, so collection still rejects distinct owned descriptions of the same graph ID.
    local function make_typed_task(p_id: string, p_type: string, p_params: any, p_options: render_graph.TaskOptions,
        p_fields: {render_graph_tasks.ResourceField}): render_graph.Task
        local uses: {render_graph.Resource} = {}
        local task_local_uses: {render_graph.Resource} = {}
        local function lower(p_value: any, p_field: render_graph_tasks.ResourceField): any
            if p_value == nil then
                return nil
            elseif p_field.kind == "resource" then
                local resource = p_value as render_graph.Resource | string
                if not (resource is string) and p_field.scope == "task_local" then
                    task_local_uses[#task_local_uses + 1] = resource
                end
                return resource_id(resource, uses)
            elseif p_field.kind == "list" then
                local result: {any} = {}
                for _, value in ipairs(p_value as {any}) do
                    result[#result + 1] = lower(value, p_field.element)
                end
                return result
            elseif p_field.kind == "map" then
                local values = p_value as {string:any}
                local result: {string:any} = {}
                for _, key in ipairs(sorted_keys(values)) do
                    result[key] = lower(values[key], p_field.element)
                end
                return result
            end
            local values = p_value as {string:any}
            local result = copy_fields(values)
            for _, field in ipairs(p_field.fields) do
                result[field.name] = lower(values[field.name], field)
            end
            return result
        end
        local params = lower(p_params, { kind = "record", fields = p_fields })

        local options: render_graph.TaskOptions = p_options or {}
        local aliases: {string:render_graph.Resource} = {}
        for local_name, resource in pairs(options.aliases or {}) do
            aliases[local_name] = resource
        end
        for _, resource in ipairs(task_local_uses) do
            local previous = aliases[resource.id]
            if previous ~= nil and previous.id ~= resource.id then
                error("render_graph: task-local resource '" .. resource.id ..
                    "' conflicts with an explicit task-local alias.", 3)
            end
            aliases[resource.id] = previous or resource
        end
        return make_task(p_id, p_type, params, {
            aliases = next(aliases) ~= nil and aliases or options.aliases,
            uses = options.uses,
            trigger = options.trigger,
            toggle = options.toggle,
        }, uses)
    end
    render_graph_tasks.install(graph, make_typed_task)

    function graph:import_task(p_id: string): render_graph.Task
        return self:add({ id = p_id, import = true })
    end

    function graph:get(p_id: string): render_graph.Task
        local task = task_by_id[p_id]
        if task == nil then
            error("render_graph: no task has the id '" .. tostring(p_id) .. "'.", 2)
        end
        validate_task(task)
        return task
    end

    local function chain(self: render_graph.Graph, p_resources: any, p_tasks: {render_graph.Task})
        local resources = connection_resources(p_resources)
        validate_list(p_tasks, "task", true)
        for _, task in ipairs(p_tasks) do
            validate_task(task)
        end
        for _, resource in ipairs(resources) do
            for i = 2, #p_tasks do
                append_edge(p_tasks[i - 1], p_tasks[i], resource)
            end
        end
    end
    graph.chain = chain

    function graph:build(p_imports?: {string}, p_ports?: {string}): wbe.RendGraphRecipeVK
        local resources: {wbe.RendRecipeResourceVK} = {}
        local wire_tasks: {wbe.RendRecipeTaskVK} = {}
        local imports: {string} = {}
        local ports: {string} = {}
        local import_ids: {string:boolean} = {}
        local port_ids: {string:boolean} = {}
        local resource_by_id: {string:render_graph.Resource} = {}
        local visiting: {render_graph.Resource:boolean} = {}
        local collected: {render_graph.Resource:boolean} = {}
        local seen_tasks: {render_graph.Task:boolean} = {}

        if #self.tasks ~= registered_count then
            error("render_graph: the registered task list was changed.", 2)
        end
        for _, task in ipairs(self.tasks) do
            validate_task(task)
            if seen_tasks[task] then
                error("render_graph: the registered task list contains a duplicate handle.", 2)
            end
            seen_tasks[task] = true
        end

        local function add_import(p_id: string)
            validate_id(p_id)
            if port_ids[p_id] then
                error("render_graph: resource '" .. p_id .. "' is both an import and a port.", 3)
            end
            if not import_ids[p_id] then
                imports[#imports + 1] = p_id
                import_ids[p_id] = true
            end
        end

        local function add_port(p_id: string)
            validate_id(p_id)
            if import_ids[p_id] then
                error("render_graph: resource '" .. p_id .. "' is both an import and a port.", 3)
            end
            if not port_ids[p_id] then
                ports[#ports + 1] = p_id
                port_ids[p_id] = true
            end
        end

        for _, id in ipairs(p_imports or {}) do
            add_import(id)
        end
        for _, id in ipairs(p_ports or {}) do
            add_port(id)
        end

        local function collect(p_resource: render_graph.Resource)
            validate_id(p_resource.id)
            if visiting[p_resource] then
                error("render_graph: resource dependency cycle at '" .. p_resource.id .. "'.", 3)
            end
            if collected[p_resource] then
                return
            end
            local previous = resource_by_id[p_resource.id]
            if previous ~= nil and previous ~= p_resource and previous.kind ~= "reference" and
                p_resource.kind ~= "reference" then
                if previous.kind ~= p_resource.kind or p_resource.description ~= nil or previous.description ~= nil then
                    error("render_graph: conflicting resource handles for '" .. p_resource.id .. "'.", 3)
                end
            end
            if previous == nil or p_resource.kind ~= "reference" then
                resource_by_id[p_resource.id] = p_resource
            end
            visiting[p_resource] = true
            for _, dependency in ipairs(p_resource.dependencies) do
                collect(dependency)
            end
            if p_resource.kind == "import" then
                add_import(p_resource.id)
            elseif p_resource.kind == "port" then
                add_port(p_resource.id)
            elseif p_resource.kind ~= "reference" then
                if import_ids[p_resource.id] or port_ids[p_resource.id] then
                    error("render_graph: owned resource '" .. p_resource.id .. "' is also an import or port.", 3)
                end
                local description = p_resource.description
                if description == nil or description.id ~= p_resource.id then
                    error("render_graph: resource '" .. p_resource.id .. "' has no matching description.", 3)
                end
                resources[#resources + 1] = description
            end
            visiting[p_resource] = nil
            collected[p_resource] = true
        end

        local function bindings(p_bindings: {string:render_graph.Resource}): {string:string}
            local result: {string:string} = {}
            for _, key in ipairs(sorted_keys(p_bindings)) do
                local resource = p_bindings[key]
                collect(resource)
                result[key] = resource.id
            end
            return result
        end

        for _, task in ipairs(self.tasks) do
            local wire_task: wbe.RendRecipeTaskVK = {
                id = task.id,
                type = task.type,
                import = task.import,
                params = task.params,
                trigger = task.trigger,
                toggle = task.toggle,
                subgraph = task.subgraph,
            }
            if task.aliases ~= nil then
                wire_task.aliases = bindings(task.aliases)
            end
            if task.edges ~= nil then
                local edges: {wbe.RendRecipeEdgeVK} = {}
                for _, edge in ipairs(task.edges) do
                    validate_task(edge.to)
                    collect(edge.resource)
                    edges[#edges + 1] = { to = edge.to.id, resource = edge.resource.id }
                end
                wire_task.connect = edges
            end
            if task.ports ~= nil then
                wire_task.ports = bindings(task.ports)
            end
            for _, resource in ipairs(task.uses or {}) do
                collect(resource)
            end
            wire_tasks[#wire_tasks + 1] = wire_task
        end
        return { imports = imports, resources = resources, tasks = wire_tasks, ports = ports }
    end
    return graph
end

return render_graph
