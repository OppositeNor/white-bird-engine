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

-- Implements the generated render_graph_utils.render_graph API for authoring render-graph recipes.
-- Handles describe a graph; Graph:build() emits the plain recipe tables C++ consumes.

local type wbe = require("wbe")
local render_graph = require("render_graph_utils.render_graph")
local render_graph_tasks = require("render_graph_utils.render_graph_tasks")

local function validate_id(identifier: string)
    if type(identifier) ~= "string" or identifier == "" then
        error("render_graph: resource and task ids must be non-empty strings.", 3)
    end
end

local function sorted_keys<T>(map: {string:T}): {string}
    local keys: {string} = {}
    for key in pairs(map) do
        keys[#keys + 1] = key
    end
    table.sort(keys)
    return keys
end

local function validate_resource(resource_handle: render_graph.Resource)
    if type(resource_handle) ~= "table" then
        error("render_graph: expected a resource handle.", 3)
    end
    validate_id(resource_handle.id)
    if type(resource_handle.kind) ~= "string" or type(resource_handle.dependencies) ~= "table" then
        error("render_graph: expected a resource handle.", 3)
    end
end

local function validate_graph_resource(resource_handle: render_graph.Resource)
    validate_resource(resource_handle)
    if resource_handle.kind == "registered" then
        error("render_graph: registered resources cannot be used as graph bindings or dependencies.", 3)
    end
end

local function validate_list<T>(input_values: {T}, name: string, allow_empty: boolean)
    if type(input_values) ~= "table" then
        error("render_graph: expected a dense " .. name .. " list.", 3)
    end
    local count = 0
    for key in pairs(input_values as {any:any}) do
        if type(key) ~= "number" then
            error("render_graph: expected a dense " .. name .. " list.", 3)
        end
        local index = key as number
        if index < 1 or index % 1 ~= 0 then
            error("render_graph: expected a dense " .. name .. " list.", 3)
        end
        count = count + 1
    end
    for i = 1, count do
        if input_values[i] == nil then
            error("render_graph: expected a dense " .. name .. " list.", 3)
        end
    end
    if count == 0 and not allow_empty then
        error("render_graph: the " .. name .. " list must not be empty.", 3)
    end
end

local function validate_graph_resource_list(resource_handles: {render_graph.Resource})
    validate_list(resource_handles, "resource", false)
    for _, resource in ipairs(resource_handles) do
        validate_graph_resource(resource)
    end
end

-- Teal cannot discriminate a record/array union; the public methods expose typed overloads instead.
local function connection_resources(resource_handles: any): {render_graph.Resource}
    if type(resource_handles) ~= "table" then
        error("render_graph: expected a resource handle, a resource bundle, or a non-empty resource list.", 3)
    end
    if (resource_handles as render_graph.Resource).id ~= nil then
        local resource = resource_handles as render_graph.Resource
        validate_graph_resource(resource)
        return { resource }
    end
    local resources = resource_handles as {render_graph.Resource}
    if (resource_handles as render_graph.ResourceBundle).resources ~= nil then
        resources = (resource_handles as render_graph.ResourceBundle).resources
    end
    validate_graph_resource_list(resources)
    return resources
end

local function append_edge(producer: render_graph.Task, consumer: render_graph.Task,
    resource_handle: render_graph.Resource)
    producer.edges = producer.edges or {}
    producer.edges[#producer.edges + 1] = { to = consumer, resource = resource_handle }
end

local function attach_task(task_handle: render_graph.Task,
    validate_task?: function(task_handle: render_graph.Task, binding_name?: string,
        resource_handle?: render_graph.Resource)): render_graph.Task
    function task_handle:bind(binding_name: string, resource_handle: render_graph.Resource): render_graph.Task
        if validate_task == nil then
            error("render_graph: add the task to a graph before binding a resource.", 2)
        end
        validate_id(binding_name)
        validate_graph_resource(resource_handle)
        validate_task(self, binding_name, resource_handle)
        self.bindings = self.bindings or {}
        if self.bindings[binding_name] ~= nil then
            error("render_graph: task-local resource '" .. binding_name .. "' is mapped more than once.", 2)
        end
        self.bindings[binding_name] = resource_handle
        return self
    end

    local function connect(self: render_graph.Task, next_task: render_graph.Task,
        resource_handles: any): render_graph.Task
        if validate_task == nil then
            error("render_graph: add the task to a graph before connecting it.", 2)
        end
        validate_task(self)
        validate_task(next_task)
        self.graph:chain(connection_resources(resource_handles), { self, next_task })
        return self
    end
    task_handle.connect = connect

    local function connect_all(self: render_graph.Task, next_tasks: {render_graph.Task},
        resource_handles: any): render_graph.Task
        if validate_task == nil then
            error("render_graph: add the task to a graph before connecting it.", 2)
        end
        validate_task(self)
        validate_list(next_tasks, "target task", false)
        for _, task in ipairs(next_tasks) do
            validate_task(task)
        end
        local resources = connection_resources(resource_handles)
        for _, resource in ipairs(resources) do
            for _, task in ipairs(next_tasks) do
                append_edge(self, task, resource)
            end
        end
        return self
    end
    task_handle.connect_all = connect_all

    function task_handle:inputs_from(producer: render_graph.Task,
        resource_bindings: {string:render_graph.Resource}): render_graph.Task
        if validate_task == nil then
            error("render_graph: add the task to a graph before assigning inputs.", 2)
        end
        validate_task(self)
        validate_task(producer)
        if type(resource_bindings) ~= "table" or next(resource_bindings) == nil then
            error("render_graph: input bindings must be a non-empty map of task-local names to resources.", 2)
        end
        for local_name, resource in pairs(resource_bindings) do
            validate_id(local_name)
            validate_graph_resource(resource)
            validate_task(self, local_name, resource)
            if self.bindings ~= nil and self.bindings[local_name] ~= nil then
                error("render_graph: task-local resource '" .. local_name .. "' is mapped more than once.", 2)
            end
        end
        self.bindings = self.bindings or {}
        local connected: {render_graph.Resource:boolean} = {}
        for _, local_name in ipairs(sorted_keys(resource_bindings)) do
            local resource = resource_bindings[local_name]
            self.bindings[local_name] = resource
            if not connected[resource] then
                append_edge(producer, self, resource)
                connected[resource] = true
            end
        end
        return self
    end

    function task_handle:color_target(target_index: integer, resource_handle: render_graph.Resource): render_graph.Task
        if type(target_index) ~= "number" or target_index < 0 or target_index % 1 ~= 0 then
            error("render_graph: color attachment indices must be non-negative integers.", 2)
        end
        return self:bind(string.format("render_target_colors[%d]", target_index), resource_handle)
    end

    function task_handle:depth_target(resource_handle: render_graph.Resource): render_graph.Task
        return self:bind("render_target_depth", resource_handle)
    end
    return task_handle
end

local function attach_resource(resource_handle: render_graph.Resource): render_graph.Resource
    local default_view: render_graph.Resource
    function resource_handle:view(identifier?: string, view?: wbe.RendImageViewParamsVK): render_graph.Resource
        if self.kind ~= "image" and self.kind ~= "import" and self.kind ~= "reference" and self.kind ~= "port" then
            error("render_graph: image views require an image resource.", 2)
        end
        if identifier == nil then
            if view ~= nil then
                error("render_graph: a customized image view requires an explicit id.", 2)
            end
            if default_view == nil then
                default_view = render_graph.image_view(self.id .. ".view", self)
            end
            return default_view
        end
        return render_graph.image_view(identifier, self, view)
    end
    return resource_handle
end

local function owned_resource(resource_description: wbe.RendRecipeResourceVK, kind: string,
    dependencies?: {render_graph.Resource}): render_graph.Resource
    validate_id(resource_description.id)
    return attach_resource({ id = resource_description.id, kind = kind, description = resource_description, dependencies = dependencies or {} })
end

-- Snapshot plain task parameters after the adapters have replaced their documented resource fields.
local function copy_data(input_value: any): any
    if type(input_value) ~= "table" then
        return input_value
    end
    local result: {any:any} = {}
    for key, value in pairs(input_value as {any:any}) do
        result[key] = copy_data(value)
    end
    return result
end

local function registered_resource(resource_key: string, type_name: string,
    resource_registration?: wbe.RendRecipeRegistrationVK): render_graph.Resource
    validate_id(resource_key)
    return attach_resource({ id = resource_key, kind = "registered", resource_type = type_name, registration = resource_registration,
        dependencies = {} })
end

local function deep_equal(left_value: any, right_value: any): boolean
    if type(left_value) ~= "table" or type(right_value) ~= "table" then
        return left_value == right_value
    end
    local left = left_value as {any:any}
    local right = right_value as {any:any}
    for key, value in pairs(left) do
        if not deep_equal(value, right[key]) then
            return false
        end
    end
    for key in pairs(right) do
        if left[key] == nil then
            return false
        end
    end
    return true
end

-- Add a registration to an ordered list unless an equal registration of its key is already there; a different
-- registration of the same key fails.
local function merge_registration(resource_registrations: {wbe.RendRecipeRegistrationVK},
    by_key: {string:wbe.RendRecipeRegistrationVK}, resource_registration: wbe.RendRecipeRegistrationVK, level: integer)
    local previous = by_key[resource_registration.key]
    if previous == nil then
        by_key[resource_registration.key] = resource_registration
        resource_registrations[#resource_registrations + 1] = resource_registration
    elseif previous.type ~= resource_registration.type or not deep_equal(previous.description, resource_registration.description) then
        error("render_graph: resource '" .. resource_registration.key .. "' is registered with different descriptions.", level)
    end
end

local function make_registration(resource_key: string, type_name: string, resource_description: any): wbe.RendRecipeRegistrationVK
    validate_id(resource_key)
    validate_id(type_name)
    return { key = resource_key, type = type_name, description = copy_data(resource_description) }
end

-- The canonical name of a reflected enum value. C++ serializes an enum as its enumerator name, and lowercase aliases
-- recipes may also write name the same enumerator.
function render_graph.enum_name(input_value: string): string
    return string.upper(input_value)
end

function render_graph.buffer(identifier: string, usage: wbe.RendResPrimaryUsageVK, size: integer): render_graph.Resource
    return owned_resource({ id = identifier, buffer = { usage = usage, size = size } }, "buffer")
end

function render_graph.storage_buffer(identifier: string, size: integer): render_graph.Resource
    return render_graph.buffer(identifier, "buffer_storage", size)
end

-- A graph-owned 2D image. Omitted extent keeps the default of wbe.RendImageParamsVK.
function render_graph.image(identifier: string, usage: wbe.RendResPrimaryUsageVK, format: wbe.ImageFormatVK,
    extent?: wbe.RendExtent2DParamsVK, additional_usage?: {wbe.ImageUsageFlagVK},
    mip_levels?: integer): render_graph.Resource
    return owned_resource({
        id = identifier,
        image = {
            usage = usage,
            format = format,
            extent = extent,
            additional_usage = additional_usage,
            mip_levels = mip_levels,
        },
    }, "image")
end

function render_graph.storage_image(identifier: string, format: wbe.ImageFormatVK, width: integer, height: integer,
    additional_usage?: {wbe.ImageUsageFlagVK}): render_graph.Resource
    return render_graph.image(identifier, "image_read_write", format, { width = width, height = height }, additional_usage)
end

-- A view keeps its source image reachable even when no task accesses the image directly.
function render_graph.image_view(identifier: string, image: render_graph.Resource,
    view?: wbe.RendImageViewParamsVK): render_graph.Resource
    if image.kind ~= "image" and image.kind ~= "import" and image.kind ~= "reference" and image.kind ~= "port" then
        error("render_graph: image views require an image resource.", 2)
    end
    return owned_resource({ id = identifier, image_view = { image = image.id, view = view } }, "image_view", { image })
end

function render_graph.clamp_to_edge_sampler(identifier: string, parameters?: wbe.RendSamplerParamsVK): render_graph.Resource
    local params: wbe.RendSamplerParamsVK = parameters or {}
    params.address_mode_u = "clamp_to_edge"
    params.address_mode_v = "clamp_to_edge"
    params.address_mode_w = "clamp_to_edge"
    return owned_resource({ id = identifier, sampler = params }, "sampler")
end

-- Pipelines are resources registered on the render graph under resource_key.
-- With a description, the handle registers the pipeline: a graph emits the registration when one of its tasks names the
-- handle.
function render_graph.compute_pipeline(resource_key: string, resource_description?: wbe.RDParamsComputePipelineVK): render_graph.Resource
    local registration = resource_description ~= nil and make_registration(resource_key, "RDParamsComputePipelineVK", resource_description) or nil
    return registered_resource(resource_key, "WhiteBirdEngine::ROComputePipelineVK", registration)
end

function render_graph.graphics_pipeline(resource_key: string, resource_description?: wbe.RDParamsGraphicsPipelineVK): render_graph.Resource
    local registration = resource_description ~= nil and make_registration(resource_key, "RDParamsGraphicsPipelineVK", resource_description) or nil
    return registered_resource(resource_key, "WhiteBirdEngine::ROGraphicsPipelineVK", registration)
end

-- A compute pipeline compiled from the shader shader_key and registered under the same key.
function render_graph.shader_compute_pipeline(shader_key: string): render_graph.Resource
    return render_graph.compute_pipeline(shader_key, { shader = shader_key })
end

-- External resources are supplied by C++; a repeated import handle with the same id denotes the same import.
function render_graph.import_resource(identifier: string): render_graph.Resource
    validate_id(identifier)
    return attach_resource({ id = identifier, kind = "import", dependencies = {} })
end

-- Ports belong to the nested recipe and are bound by its enclosing subgraph task.
function render_graph.port(identifier: string): render_graph.Resource
    validate_id(identifier)
    return attach_resource({ id = identifier, kind = "port", dependencies = {} })
end

-- Reference an enclosing resource or a member of an imported composite without declaring another import.
-- C++ resolves and validates the id when it assembles the graph.
function render_graph.reference(identifier: string): render_graph.Resource
    validate_id(identifier)
    return attach_resource({ id = identifier, kind = "reference", dependencies = {} })
end

-- A dispatch accepts graph handles or task-local strings, resolved inside the containing task's handle bindings.
function render_graph.indirect_dispatch(pipeline_handle: render_graph.Resource,
    indirect_buffer: render_graph.Resource, offset: integer,
    resource_bindings?: {string:render_graph.Resource}): render_graph.RTParamsComputeDispatchVK
    return { pipeline = pipeline_handle, indirect_buffer = indirect_buffer, indirect_offset = offset, bindings = resource_bindings }
end

function render_graph.subgraph(identifier: string, recipe: wbe.RendGraphRecipeVK,
    explicit_ports: {string:render_graph.Resource}): render_graph.Task
    return attach_task({ id = identifier, subgraph = recipe, ports = explicit_ports })
end

local function copy_fields<T>(input_value: T): {string:any}
    local result: {string:any} = {}
    for key, value in pairs(input_value as {string:any}) do
        result[key] = value
    end
    return result
end

local function resource_id(input_value: render_graph.Resource, additional_uses: {render_graph.Resource}): string
    if input_value == nil then
        return nil
    end
    validate_graph_resource(input_value)
    additional_uses[#additional_uses + 1] = input_value
    return input_value.id
end

-- Lower a registered handle to its key, collecting the registration it carries into resource_registrations.
local function registered_key(input_value: any, expected_type: string, resource_registrations: {wbe.RendRecipeRegistrationVK}): string
    local resource = input_value as render_graph.Resource
    validate_resource(resource)
    if resource.kind ~= "registered" or resource.resource_type ~= expected_type then
        error("render_graph: expected a registered resource of type '" .. expected_type .. "'.", 3)
    end
    if resource.registration ~= nil then
        resource_registrations[#resource_registrations + 1] = resource.registration
    end
    return resource.id
end

function render_graph.graph(): render_graph.Graph
    local tasks: {render_graph.Task} = {}
    local task_by_id: {string:render_graph.Task} = {}
    local registered_ids: {render_graph.Task:string} = {}
    local parameter_ids: {render_graph.Task:{string:boolean}} = {}
    local registered_count = 0
    local registrations: {wbe.RendRecipeRegistrationVK} = {}
    local registrations_by_key: {string:wbe.RendRecipeRegistrationVK} = {}
    local graph: render_graph.Graph = { tasks = tasks }

    -- Register a resource on the render graph when the recipe is built. The description's reflected type name selects the
    -- registrator. Explicit registrations are emitted in order, before any task, whether or not a task uses them; an equal
    -- registration of a key again is merged, and a different one fails.
    local function add_registration(resource_registration: wbe.RendRecipeRegistrationVK)
        merge_registration(registrations, registrations_by_key, resource_registration, 4)
    end

    function graph:register_resource(resource_key: string, type_name: string, resource_description: any,
        resource_type: string): render_graph.Resource
        local registration = make_registration(resource_key, type_name, resource_description)
        add_registration(registration)
        return registered_resource(resource_key, resource_type, registration)
    end

    function graph:register_mesh(resource_key: string, parameters: wbe.RDParamsMeshVK): render_graph.Resource
        local registration = make_registration(resource_key, "RDParamsMeshVK", parameters)
        add_registration(registration)
        return registered_resource(resource_key, "WhiteBirdEngine::MeshResourcesVK", registration)
    end

    function graph:register_graphics_pipeline(resource_key: string,
        parameters: wbe.RDParamsGraphicsPipelineVK): render_graph.Resource
        local pipeline = render_graph.graphics_pipeline(resource_key, parameters)
        add_registration(pipeline.registration)
        return pipeline
    end

    function graph:register_compute_pipeline(resource_key: string,
        parameters: wbe.RDParamsComputePipelineVK): render_graph.Resource
        local pipeline = render_graph.compute_pipeline(resource_key, parameters)
        add_registration(pipeline.registration)
        return pipeline
    end

    -- A registered image is bound like an import: C++ resolves an unsupplied import to the registered view.
    function graph:register_image(resource_key: string, parameters: wbe.RDParamsImageVK): render_graph.Resource
        add_registration(make_registration(resource_key, "RDParamsImageVK", parameters))
        return render_graph.import_resource(resource_key)
    end

    function graph:resource_bundle(resource_handles: {render_graph.Resource}): render_graph.ResourceBundle
        validate_graph_resource_list(resource_handles)
        local resources: {render_graph.Resource} = {}
        for _, resource in ipairs(resource_handles) do
            resources[#resources + 1] = resource
        end
        return { resources = resources }
    end

    -- Declare entries registered on the render graph as resources. Their task factories resolve the live objects in C++.
    function graph:add_mesh(resource_key: string): render_graph.Resource
        return registered_resource(resource_key, "WhiteBirdEngine::MeshResourcesVK")
    end

    function graph:add_compute_pipeline(resource_key: string): render_graph.Resource
        return render_graph.compute_pipeline(resource_key)
    end

    function graph:add_graphics_pipeline(resource_key: string): render_graph.Resource
        return render_graph.graphics_pipeline(resource_key)
    end

    -- Graph handles describe resources lazily and may be shared with other graph descriptions.
    function graph:add_buffer(identifier: string, usage: wbe.RendResPrimaryUsageVK, size: integer): render_graph.Resource
        return render_graph.buffer(identifier, usage, size)
    end

    function graph:add_storage_buffer(identifier: string, size: integer): render_graph.Resource
        return render_graph.storage_buffer(identifier, size)
    end

    function graph:add_image(identifier: string, usage: wbe.RendResPrimaryUsageVK, format: wbe.ImageFormatVK,
        extent?: wbe.RendExtent2DParamsVK, additional_usage?: {wbe.ImageUsageFlagVK},
        mip_levels?: integer): render_graph.Resource
        return render_graph.image(identifier, usage, format, extent, additional_usage, mip_levels)
    end

    function graph:add_storage_image(identifier: string, format: wbe.ImageFormatVK, width: integer, height: integer,
        additional_usage?: {wbe.ImageUsageFlagVK}): render_graph.Resource
        return render_graph.storage_image(identifier, format, width, height, additional_usage)
    end

    function graph:add_image_view(identifier: string, image: render_graph.Resource,
        view?: wbe.RendImageViewParamsVK): render_graph.Resource
        return render_graph.image_view(identifier, image, view)
    end

    function graph:add_clamp_to_edge_sampler(identifier: string, parameters?: wbe.RendSamplerParamsVK): render_graph.Resource
        return render_graph.clamp_to_edge_sampler(identifier, parameters)
    end

    function graph:import_resource(identifier: string): render_graph.Resource
        return render_graph.import_resource(identifier)
    end

    function graph:port(identifier: string): render_graph.Resource
        return render_graph.port(identifier)
    end

    function graph:reference(identifier: string): render_graph.Resource
        return render_graph.reference(identifier)
    end

    function graph:indirect_dispatch(pipeline_handle: render_graph.Resource,
        indirect_buffer: render_graph.Resource, offset: integer,
        resource_bindings?: {string:render_graph.Resource}): render_graph.RTParamsComputeDispatchVK
        return render_graph.indirect_dispatch(pipeline_handle, indirect_buffer, offset, resource_bindings)
    end

    function graph:as_subgraph(identifier: string, explicit_ports: {string:render_graph.Resource},
        explicit_imports?: {string}): render_graph.Task
        return render_graph.subgraph(identifier, self:build(explicit_imports, sorted_keys(explicit_ports)), explicit_ports)
    end

    local function validate_parameter_binding(task_handle: render_graph.Task, binding_name: string,
        resource_handle: render_graph.Resource)
        local ids = parameter_ids[task_handle]
        if ids ~= nil and ids[binding_name] and resource_handle.id ~= binding_name then
            error("render_graph: task parameter resource '" .. binding_name .. "' conflicts with a task binding.", 3)
        end
    end

    local function validate_parameter_bindings(task_handle: render_graph.Task)
        for local_name, resource in pairs(task_handle.bindings or {}) do
            validate_parameter_binding(task_handle, local_name, resource)
        end
    end

    local function validate_task(task_handle: render_graph.Task, binding_name?: string,
        resource_handle?: render_graph.Resource)
        if type(task_handle) ~= "table" then
            error("render_graph: expected a task handle belonging to this graph.", 3)
        end
        local id = registered_ids[task_handle]
        if task_handle.graph ~= graph or id == nil or task_by_id[id] ~= task_handle then
            error("render_graph: task handle does not belong to this graph.", 3)
        end
        if task_handle.id ~= id then
            error("render_graph: the registered task id '" .. id .. "' was changed.", 3)
        end
        validate_parameter_bindings(task_handle)
        if binding_name ~= nil then
            validate_parameter_binding(task_handle, binding_name, resource_handle)
        end
    end

    function graph:add(task_handle: render_graph.Task): render_graph.Task
        validate_id(task_handle.id)
        if task_handle.graph ~= nil and task_handle.graph ~= self then
            error("render_graph: task '" .. task_handle.id .. "' already belongs to another graph.", 2)
        end
        if task_by_id[task_handle.id] ~= nil then
            error("render_graph: the task id '" .. task_handle.id .. "' is added twice.", 2)
        end
        if registered_ids[task_handle] ~= nil then
            error("render_graph: the registered task id was changed before adding it again.", 2)
        end
        if task_handle.bindings ~= nil then
            local bindings: {string:render_graph.Resource} = {}
            for local_name, resource in pairs(task_handle.bindings) do
                bindings[local_name] = resource
            end
            task_handle.bindings = bindings
        end
        if task_handle.edges ~= nil then
            local edges: {render_graph.Edge} = {}
            for _, edge in ipairs(task_handle.edges) do
                edges[#edges + 1] = { to = edge.to, resource = edge.resource }
            end
            task_handle.edges = edges
        end
        attach_task(task_handle, validate_task)
        task_handle.graph = self
        self.tasks[#self.tasks + 1] = task_handle
        task_by_id[task_handle.id] = task_handle
        registered_ids[task_handle] = task_handle.id
        registered_count = registered_count + 1
        return task_handle
    end

    local function make_task(identifier: string, type_name: string, parameters: any,
        settings?: render_graph.TaskOptions, additional_uses?: {render_graph.Resource},
        resource_registrations?: {wbe.RendRecipeRegistrationVK}): render_graph.Task
        local options: render_graph.TaskOptions = settings or {}
        local uses: {render_graph.Resource} = {}
        for _, resource in ipairs(options.uses or {}) do
            uses[#uses + 1] = resource
        end
        for _, resource in ipairs(additional_uses or {}) do
            uses[#uses + 1] = resource
        end
        local task: render_graph.Task = {
            id = identifier,
            type = type_name,
            params = copy_data(parameters),
            bindings = options.bindings,
            uses = uses,
            trigger = copy_data(options.trigger) as wbe.RendRecipeTriggerVK,
            toggle = copy_data(options.toggle) as wbe.RendRecipeToggleVK,
            registrations = resource_registrations,
        }
        local ids: {string:boolean} = {}
        for _, resource in ipairs(additional_uses or {}) do
            ids[resource.id] = true
        end
        parameter_ids[task] = ids
        validate_parameter_bindings(task)
        return graph:add(task)
    end

    -- The typed constructors lower registered keys to strings and graph resource handles to IDs, retaining only the latter.
    -- A task-local handle also binds its graph ID as an identity binding, which cannot shadow an unrelated shader binding.
    -- Every handle stays reachable, so collection still rejects distinct owned descriptions of the same graph ID.
    local function make_typed_task(identifier: string, type_name: string, parameters: any, settings: render_graph.TaskOptions,
        fields: {render_graph_tasks.ResourceField}): render_graph.Task
        local uses: {render_graph.Resource} = {}
        local task_local_uses: {render_graph.Resource} = {}
        local task_registrations: {wbe.RendRecipeRegistrationVK} = {}
        local function lower(input_value: any, resource_field: render_graph_tasks.ResourceField): any
            if input_value == nil then
                return nil
            elseif resource_field.kind == "resource" then
                if resource_field.scope == "registered" then
                    return registered_key(input_value, resource_field.expected_type, task_registrations)
                end
                if input_value is string then
                    error("render_graph: resource fields take handles; name '" .. input_value ..
                        "' with graph:reference().", 4)
                end
                local resource = input_value as render_graph.Resource
                if resource_field.scope == "task_local" then
                    task_local_uses[#task_local_uses + 1] = resource
                end
                return resource_id(resource, uses)
            elseif resource_field.kind == "list" then
                local result: {any} = {}
                for _, value in ipairs(input_value as {any}) do
                    result[#result + 1] = lower(value, resource_field.element)
                end
                return result
            elseif resource_field.kind == "map" then
                local values = input_value as {string:any}
                local result: {string:any} = {}
                for _, key in ipairs(sorted_keys(values)) do
                    result[key] = lower(values[key], resource_field.element)
                end
                return result
            end
            local values = input_value as {string:any}
            local result = copy_fields(values)
            for _, field in ipairs(resource_field.fields) do
                result[field.name] = lower(values[field.name], field)
            end
            return result
        end
        local params = lower(parameters, { kind = "record", fields = fields })

        local options: render_graph.TaskOptions = settings or {}
        local bindings: {string:render_graph.Resource} = {}
        for local_name, resource in pairs(options.bindings or {}) do
            bindings[local_name] = resource
        end
        for _, resource in ipairs(task_local_uses) do
            local previous = bindings[resource.id]
            if previous ~= nil and previous.id ~= resource.id then
                error("render_graph: task-local resource '" .. resource.id ..
                    "' conflicts with an explicit task binding.", 3)
            end
            bindings[resource.id] = previous or resource
        end
        return make_task(identifier, type_name, params, {
            bindings = next(bindings) ~= nil and bindings or options.bindings,
            uses = options.uses,
            trigger = options.trigger,
            toggle = options.toggle,
        }, uses, task_registrations)
    end
    render_graph_tasks.install(graph, make_typed_task)

    function graph:import_task(identifier: string): render_graph.Task
        return self:add({ id = identifier, import = true })
    end

    function graph:multiplex(identifier: string, branch_tasks: {render_graph.Task},
        settings: render_graph.MultiplexOptions): render_graph.Task
        validate_list(branch_tasks, "branch", false)
        if settings == nil or settings.select == nil then
            error("render_graph: multiplexer '" .. tostring(identifier) .. "' sets no select.", 2)
        end
        local bindings: {string:render_graph.Resource} = {}
        local function add_binding(binding_name: string, resource_handle: render_graph.Resource)
            local previous = bindings[binding_name]
            if previous ~= nil and previous.id ~= resource_handle.id then
                error("render_graph: multiplexer '" .. identifier .. "' maps task-local resource '" .. binding_name ..
                    "' to more than one graph resource.", 3)
            end
            bindings[binding_name] = previous or resource_handle
        end
        for local_name, resource in pairs(settings.bindings or {}) do
            add_binding(local_name, resource)
        end
        local uses: {render_graph.Resource} = {}
        for _, resource in ipairs(settings.uses or {}) do
            uses[#uses + 1] = resource
        end
        -- The branches' parameter handles keep their identity bindings from being redirected.
        local ids: {string:boolean} = {}
        local branches: {wbe.RendRecipeBranchVK} = {}
        -- The branches were built on another graph, so the multiplexer carries their registrations into this one.
        local branch_registrations: {wbe.RendRecipeRegistrationVK} = {}
        for index, branch in ipairs(branch_tasks) do
            if branch.graph == self then
                error("render_graph: multiplexer '" .. identifier .. "' branch " .. index ..
                    " belongs to the same graph; build branches on a separate graph.", 2)
            end
            if branch.type == nil or branch.import or branch.subgraph ~= nil or branch.branches ~= nil then
                error("render_graph: multiplexer '" .. identifier .. "' branch " .. index .. " is not a typed task.", 2)
            end
            if branch.edges ~= nil or branch.trigger ~= nil or branch.toggle ~= nil then
                error("render_graph: multiplexer '" .. identifier .. "' branch " .. index ..
                    " connects, triggers, or toggles; set those on the multiplexer.", 2)
            end
            branches[#branches + 1] = { type = branch.type, params = copy_data(branch.params) }
            for local_name, resource in pairs(branch.bindings or {}) do
                add_binding(local_name, resource)
            end
            for _, resource in ipairs(branch.uses or {}) do
                uses[#uses + 1] = resource
                ids[resource.id] = true
            end
            for _, registration in ipairs(branch.registrations or {}) do
                branch_registrations[#branch_registrations + 1] = registration
            end
        end
        local task: render_graph.Task = {
            id = identifier,
            branches = branches,
            select = copy_data(settings.select) as wbe.RendRecipeSelectVK,
            bindings = next(bindings) ~= nil and bindings or nil,
            uses = uses,
            trigger = copy_data(settings.trigger) as wbe.RendRecipeTriggerVK,
            toggle = copy_data(settings.toggle) as wbe.RendRecipeToggleVK,
            registrations = branch_registrations,
        }
        parameter_ids[task] = ids
        validate_parameter_bindings(task)
        return graph:add(task)
    end

    function graph:get(identifier: string): render_graph.Task
        local task = task_by_id[identifier]
        if task == nil then
            error("render_graph: no task has the id '" .. tostring(identifier) .. "'.", 2)
        end
        validate_task(task)
        return task
    end

    local function chain(_: render_graph.Graph, resource_handles: any, task_handles: {render_graph.Task})
        local resources = connection_resources(resource_handles)
        validate_list(task_handles, "task", true)
        for _, task in ipairs(task_handles) do
            validate_task(task)
        end
        for _, resource in ipairs(resources) do
            for i = 2, #task_handles do
                append_edge(task_handles[i - 1], task_handles[i], resource)
            end
        end
    end
    graph.chain = chain

    function graph:build(explicit_imports?: {string}, explicit_ports?: {string}): wbe.RendGraphRecipeVK
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

        local function add_import(identifier: string)
            validate_id(identifier)
            if port_ids[identifier] then
                error("render_graph: resource '" .. identifier .. "' is both an import and a port.", 3)
            end
            if not import_ids[identifier] then
                imports[#imports + 1] = identifier
                import_ids[identifier] = true
            end
        end

        local function add_port(identifier: string)
            validate_id(identifier)
            if import_ids[identifier] then
                error("render_graph: resource '" .. identifier .. "' is both an import and a port.", 3)
            end
            if not port_ids[identifier] then
                ports[#ports + 1] = identifier
                port_ids[identifier] = true
            end
        end

        for _, id in ipairs(explicit_imports or {}) do
            add_import(id)
        end
        for _, id in ipairs(explicit_ports or {}) do
            add_port(id)
        end

        local function collect(resource_handle: render_graph.Resource)
            validate_graph_resource(resource_handle)
            if visiting[resource_handle] then
                error("render_graph: resource dependency cycle at '" .. resource_handle.id .. "'.", 3)
            end
            if collected[resource_handle] then
                return
            end
            local previous = resource_by_id[resource_handle.id]
            if previous ~= nil and previous ~= resource_handle and previous.kind ~= "reference" and
                resource_handle.kind ~= "reference" then
                if previous.kind ~= resource_handle.kind or resource_handle.description ~= nil or previous.description ~= nil then
                    error("render_graph: conflicting resource handles for '" .. resource_handle.id .. "'.", 3)
                end
            end
            if previous == nil or resource_handle.kind ~= "reference" then
                resource_by_id[resource_handle.id] = resource_handle
            end
            visiting[resource_handle] = true
            for _, dependency in ipairs(resource_handle.dependencies) do
                collect(dependency)
            end
            if resource_handle.kind == "import" then
                add_import(resource_handle.id)
            elseif resource_handle.kind == "port" then
                add_port(resource_handle.id)
            elseif resource_handle.kind ~= "reference" then
                if import_ids[resource_handle.id] or port_ids[resource_handle.id] then
                    error("render_graph: owned resource '" .. resource_handle.id .. "' is also an import or port.", 3)
                end
                local description = resource_handle.description
                if description == nil or description.id ~= resource_handle.id then
                    error("render_graph: resource '" .. resource_handle.id .. "' has no matching description.", 3)
                end
                resources[#resources + 1] = description
            end
            visiting[resource_handle] = nil
            collected[resource_handle] = true
        end

        local function resource_ids(resource_bindings: {string:render_graph.Resource}): {string:string}
            local result: {string:string} = {}
            for _, key in ipairs(sorted_keys(resource_bindings)) do
                local resource = resource_bindings[key]
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
                branches = task.branches,
                select = task.select,
            }
            if task.bindings ~= nil then
                wire_task.aliases = resource_ids(task.bindings)
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
                wire_task.ports = resource_ids(task.ports)
            end
            for _, resource in ipairs(task.uses or {}) do
                collect(resource)
            end
            wire_tasks[#wire_tasks + 1] = wire_task
        end
        -- The explicit registrations, then those of the registered handles the tasks name, each key once.
        local wire_registrations: {wbe.RendRecipeRegistrationVK} = {}
        local wire_registrations_by_key: {string:wbe.RendRecipeRegistrationVK} = {}
        for _, registration in ipairs(registrations) do
            merge_registration(wire_registrations, wire_registrations_by_key, registration, 2)
        end
        for _, task in ipairs(self.tasks) do
            for _, registration in ipairs(task.registrations or {}) do
                merge_registration(wire_registrations, wire_registrations_by_key, registration, 2)
            end
        end
        return { imports = imports, registrations = wire_registrations, resources = resources, tasks = wire_tasks,
            ports = ports }
    end
    return graph
end

function render_graph.new(): render_graph.Graph
    return render_graph.graph()
end

return render_graph
