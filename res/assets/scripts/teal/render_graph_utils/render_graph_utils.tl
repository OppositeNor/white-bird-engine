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

-- Implements the generated render_graph_utils.render_graph API for authoring render-graph recipes. A recipe describes its
-- graph with handles: resources and tasks are free-standing descriptions, and graph.build() generates the ids of the
-- declared resources and tasks it reaches and emits the plain recipe tables C++ consumes.

local type wbe = require("wbe")
local graph = require("render_graph_utils.render_graph")
local render_graph_tasks = require("render_graph_utils.render_graph_tasks")

-- Every resource and task handle, so that snapshots of task params keep handles instead of copying them. Weak, since a
-- handle is only state of the recipe run that created it.
local resource_handles: {any:boolean} = setmetatable({}, { __mode = "k" })
local task_handles: {any:boolean} = setmetatable({}, { __mode = "k" })

local function validate_name(value: string, what: string)
    if type(value) ~= "string" or value == "" then
        error("render_graph: " .. what .. " must be a non-empty string.", 3)
    end
end

local function validate_optional_name(value: string)
    if value ~= nil then
        validate_name(value, "a debug name")
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

local function is_resource(value: any): boolean
    return resource_handles[value] == true
end

local function is_task(value: any): boolean
    return task_handles[value] == true
end

local function validate_resource(resource_handle: graph.Resource)
    if not is_resource(resource_handle) then
        error("render_graph: expected a resource handle.", 3)
    end
end

local function validate_graph_resource(resource_handle: graph.Resource)
    validate_resource(resource_handle)
    if resource_handle.kind == "registered" then
        error("render_graph: registered resources cannot be used as graph bindings or dependencies.", 3)
    end
end

local function validate_task(task_handle: graph.Task)
    if not is_task(task_handle) then
        error("render_graph: expected a task handle.", 3)
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

-- Teal cannot discriminate a record/array union; the public functions expose typed overloads instead.
local function connection_resources(resources: any): {graph.Resource}
    if is_resource(resources) then
        local resource = resources as graph.Resource
        validate_graph_resource(resource)
        return { resource }
    end
    if type(resources) ~= "table" then
        error("render_graph: expected a resource handle or a non-empty resource list.", 3)
    end
    local list = resources as {graph.Resource}
    validate_list(list, "resource", false)
    for _, resource in ipairs(list) do
        validate_graph_resource(resource)
    end
    return list
end

-- Copy plain data, keeping the handles it holds.
local function snapshot(input_value: any): any
    if type(input_value) ~= "table" or is_resource(input_value) or is_task(input_value) then
        return input_value
    end
    local result: {any:any} = {}
    for key, value in pairs(input_value as {any:any}) do
        result[key] = snapshot(value)
    end
    return result
end

local function contains_handle(input_value: any): boolean
    if is_resource(input_value) or is_task(input_value) then
        return true
    end
    if type(input_value) ~= "table" then
        return false
    end
    for _, value in pairs(input_value as {any:any}) do
        if contains_handle(value) then
            return true
        end
    end
    return false
end

local function copy_bindings(resource_bindings: {string:graph.Resource}): {string:graph.Resource}
    local result: {string:graph.Resource} = {}
    for local_name, resource in pairs(resource_bindings or {}) do
        validate_name(local_name, "a task-local resource name")
        validate_graph_resource(resource)
        result[local_name] = resource
    end
    return result
end

local function append_edge(producer: graph.Task, consumer: graph.Task, resource_handle: graph.Resource)
    producer.edges[#producer.edges + 1] = { to = consumer, resource = resource_handle }
end

local function new_task(task_handle: graph.Task): graph.Task
    validate_optional_name(task_handle.name)
    task_handle.bindings = task_handle.bindings or {}
    task_handle.edges = {}
    task_handles[task_handle] = true

    function task_handle:bind(binding_name: string, resource_handle: graph.Resource): graph.Task
        validate_name(binding_name, "a task-local resource name")
        validate_graph_resource(resource_handle)
        if self.bindings[binding_name] ~= nil then
            error("render_graph: task-local resource '" .. binding_name .. "' is bound more than once.", 2)
        end
        self.bindings[binding_name] = resource_handle
        return self
    end

    local function connect(self: graph.Task, next_task: graph.Task, resources: any): graph.Task
        validate_task(next_task)
        for _, resource in ipairs(connection_resources(resources)) do
            append_edge(self, next_task, resource)
        end
        return self
    end
    task_handle.connect = connect

    local function connect_all(self: graph.Task, next_tasks: {graph.Task}, resources: any): graph.Task
        validate_list(next_tasks, "target task", false)
        for _, task in ipairs(next_tasks) do
            validate_task(task)
        end
        for _, resource in ipairs(connection_resources(resources)) do
            for _, task in ipairs(next_tasks) do
                append_edge(self, task, resource)
            end
        end
        return self
    end
    task_handle.connect_all = connect_all

    function task_handle:inputs_from(producer: graph.Task, resource_bindings: {string:graph.Resource}): graph.Task
        validate_task(producer)
        if type(resource_bindings) ~= "table" or next(resource_bindings) == nil then
            error("render_graph: input bindings must be a non-empty map of task-local names to resources.", 2)
        end
        for local_name, resource in pairs(resource_bindings) do
            validate_name(local_name, "a task-local resource name")
            validate_graph_resource(resource)
            if self.bindings[local_name] ~= nil then
                error("render_graph: task-local resource '" .. local_name .. "' is bound more than once.", 2)
            end
        end
        local connected: {graph.Resource:boolean} = {}
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

    function task_handle:color_target(target_index: integer, resource_handle: graph.Resource): graph.Task
        if type(target_index) ~= "number" or target_index < 0 or target_index % 1 ~= 0 then
            error("render_graph: color attachment indices must be non-negative integers.", 2)
        end
        return self:bind(string.format("render_target_colors[%d]", target_index), resource_handle)
    end

    function task_handle:depth_target(resource_handle: graph.Resource): graph.Task
        return self:bind("render_target_depth", resource_handle)
    end
    return task_handle
end

local function is_image(resource_handle: graph.Resource): boolean
    local kind = resource_handle.kind
    return kind == "image" or kind == "import" or kind == "member" or kind == "port"
end

local function new_resource(resource_handle: graph.Resource): graph.Resource
    resource_handle.dependencies = resource_handle.dependencies or {}
    resource_handles[resource_handle] = true
    local default_view: graph.Resource
    function resource_handle:view(view?: graph.ImageViewDescriptor): graph.Resource
        if view ~= nil then
            return graph.image_view(self, view)
        end
        if default_view == nil then
            -- The default view takes the image's debug name.
            default_view = graph.image_view(self, { name = self.name })
        end
        return default_view
    end
    function resource_handle:member(member_name: string): graph.Resource
        if self.kind ~= "import" then
            error("render_graph: only imports have members.", 2)
        end
        validate_name(member_name, "a member name")
        return new_resource({ kind = "member", key = self.key .. "." .. member_name, dependencies = { self } })
    end
    return resource_handle
end

-- The reflected parameters of a descriptor, without its debug name, and the debug name.
local function descriptor_params(descriptor: any): {string:any}, string
    local params: {string:any} = {}
    for key, value in pairs((descriptor or {}) as {string:any}) do
        params[key] = snapshot(value)
    end
    local name = params.name as string
    params.name = nil
    validate_optional_name(name)
    return params, name
end

local function declared_resource(kind: string, name: string, description: wbe.RendRecipeResourceVK,
    dependencies?: {graph.Resource}): graph.Resource
    return new_resource({ kind = kind, name = name, description = description, dependencies = dependencies })
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

local function make_registration(resource_key: string, type_name: string, resource_description: any): wbe.RendRecipeRegistrationVK
    validate_name(resource_key, "a registration key")
    validate_name(type_name, "a description type name")
    if contains_handle(resource_description) then
        error("render_graph: registration descriptions hold no handles.", 3)
    end
    return { key = resource_key, type = type_name, description = snapshot(resource_description) }
end

local function registered_resource(resource_key: string, type_name: string,
    resource_registration?: wbe.RendRecipeRegistrationVK): graph.Resource
    validate_name(resource_key, "a registration key")
    return new_resource({ kind = "registered", key = resource_key, resource_type = type_name,
        registration = resource_registration })
end

-- 32-bit FNV-1a of text.
local function hash(text: string): integer
    local result = 0x811c9dc5
    for i = 1, #text do
        result = ((result ~ string.byte(text, i)) * 0x01000193) & 0xffffffff
    end
    return result
end

-- Check a typed task's params against its resource fields when the task is created, so that a wrong value fails at the
-- call that passes it.
local function check_params(parameters: any, fields: {render_graph_tasks.ResourceField})
    local function check(input_value: any, resource_field: render_graph_tasks.ResourceField)
        if input_value == nil then
            return
        elseif resource_field.kind == "resource" then
            if not is_resource(input_value) then
                error("render_graph: resource field '" .. tostring(resource_field.name) .. "' takes a resource handle.", 5)
            end
            local resource = input_value as graph.Resource
            if resource_field.scope == "registered" then
                if resource.kind ~= "registered" or resource.resource_type ~= resource_field.expected_type then
                    error("render_graph: expected a registered resource of type '" .. resource_field.expected_type .. "'.", 5)
                end
            elseif resource.kind == "registered" then
                error("render_graph: registered resources cannot be used as graph bindings or dependencies.", 5)
            end
        elseif resource_field.kind == "list" then
            for _, value in ipairs(input_value as {any}) do
                check(value, resource_field.element)
            end
        elseif resource_field.kind == "map" then
            for _, value in pairs(input_value as {string:any}) do
                check(value, resource_field.element)
            end
        else
            local values = input_value as {string:any}
            for _, field in ipairs(resource_field.fields) do
                check(values[field.name], field)
            end
        end
    end
    check(parameters, { kind = "record", fields = fields })
end

local function gating_options(options: graph.TaskOptions): wbe.RendRecipeTriggerVK, wbe.RendRecipeToggleVK
    return snapshot(options.trigger) as wbe.RendRecipeTriggerVK, snapshot(options.toggle) as wbe.RendRecipeToggleVK
end

-- The typed constructors keep the handles in their params; the build lowers them with the resource fields.
local function make_typed_task(type_name: string, parameters: any, settings: graph.TaskOptions,
    fields: {render_graph_tasks.ResourceField}): graph.Task
    check_params(parameters, fields)
    local options: graph.TaskOptions = settings or {}
    local trigger, toggle = gating_options(options)
    return new_task({
        kind = "typed",
        name = options.name,
        type = type_name,
        params = snapshot(parameters),
        fields = fields,
        bindings = copy_bindings(options.bindings),
        trigger = trigger,
        toggle = toggle,
    })
end
render_graph_tasks.install(graph, make_typed_task)

-- The canonical name of a reflected enum value. C++ serializes an enum as its enumerator name, and lowercase aliases
-- recipes may also write name the same enumerator.
function graph.enum_name(input_value: string): string
    return string.upper(input_value)
end

function graph.buffer(descriptor: graph.BufferDescriptor): graph.Resource
    local params, name = descriptor_params(descriptor)
    return declared_resource("buffer", name, { buffer = (params as any) as wbe.RendBufferParamsVK })
end

function graph.storage_buffer(descriptor: graph.BufferDescriptor): graph.Resource
    if descriptor.usage ~= nil then
        error("render_graph: storage_buffer() sets the usage; declare other buffers with buffer().", 2)
    end
    local params, name = descriptor_params(descriptor)
    params.usage = "buffer_storage"
    return declared_resource("buffer", name, { buffer = (params as any) as wbe.RendBufferParamsVK })
end

-- A graph-owned image. Omitted fields keep the defaults of wbe.RendImageParamsVK.
function graph.image(descriptor: graph.ImageDescriptor): graph.Resource
    local params, name = descriptor_params(descriptor)
    return declared_resource("image", name, { image = (params as any) as wbe.RendImageParamsVK })
end

function graph.storage_image(descriptor: graph.ImageDescriptor): graph.Resource
    if descriptor.usage ~= nil then
        error("render_graph: storage_image() sets the usage; declare other images with image().", 2)
    end
    local params, name = descriptor_params(descriptor)
    params.usage = "image_read_write"
    return declared_resource("image", name, { image = (params as any) as wbe.RendImageParamsVK })
end

-- A view keeps its source image reachable even when no task accesses the image directly. The build fills in the image's
-- id.
function graph.image_view(image: graph.Resource, descriptor?: graph.ImageViewDescriptor): graph.Resource
    validate_resource(image)
    if not is_image(image) then
        error("render_graph: image views require an image resource.", 2)
    end
    local params, name = descriptor_params(descriptor)
    return declared_resource("image_view", name,
        { image_view = { image = "", view = (params as any) as wbe.RendImageViewParamsVK } }, { image })
end

function graph.clamp_to_edge_sampler(descriptor?: graph.SamplerDescriptor): graph.Resource
    local params, name = descriptor_params(descriptor)
    params.address_mode_u = "clamp_to_edge"
    params.address_mode_v = "clamp_to_edge"
    params.address_mode_w = "clamp_to_edge"
    return declared_resource("sampler", name, { sampler = (params as any) as wbe.RendSamplerParamsVK })
end

-- A mesh registered on the render graph under resource_key. With a description, the handle registers it; without one, it
-- names a mesh registered elsewhere, and a draw naming a key nobody registered registers the mesh resource of that key
-- with the default options.
function graph.mesh(resource_key: string, resource_description?: wbe.RDParamsMeshVK): graph.Resource
    local registration = resource_description ~= nil and make_registration(resource_key, "RDParamsMeshVK", resource_description) or nil
    return registered_resource(resource_key, "WhiteBirdEngine::MeshResourcesVK", registration)
end

-- Pipelines are resources registered on the render graph under resource_key. With a description, the handle registers
-- the pipeline.
function graph.compute_pipeline(resource_key: string, resource_description?: wbe.RDParamsComputePipelineVK): graph.Resource
    local registration = resource_description ~= nil and make_registration(resource_key, "RDParamsComputePipelineVK", resource_description) or nil
    return registered_resource(resource_key, "WhiteBirdEngine::ROComputePipelineVK", registration)
end

function graph.graphics_pipeline(resource_key: string, resource_description?: wbe.RDParamsGraphicsPipelineVK): graph.Resource
    local registration = resource_description ~= nil and make_registration(resource_key, "RDParamsGraphicsPipelineVK", resource_description) or nil
    return registered_resource(resource_key, "WhiteBirdEngine::ROGraphicsPipelineVK", registration)
end

-- A compute pipeline compiled from the shader shader_key and registered under the same key.
function graph.shader_compute_pipeline(shader_key: string): graph.Resource
    return graph.compute_pipeline(shader_key, { shader = shader_key })
end

-- A resource of a custom description type, registered under resource_key: type_name is the reflected name of the
-- description struct, which selects the registrator, and resource_type the C++ type of the registered object.
function graph.registered_resource(resource_key: string, type_name: string, resource_description: any,
    resource_type: string): graph.Resource
    return registered_resource(resource_key, resource_type, make_registration(resource_key, type_name, resource_description))
end

-- A resource C++ supplies under identifier; import handles of the same identifier denote the same import.
function graph.import(identifier: string): graph.Resource
    validate_name(identifier, "an import id")
    return new_resource({ kind = "import", key = identifier })
end

-- An image registered on the render graph under resource_key and bound like an import: C++ resolves the unsupplied
-- import to the registered view.
function graph.registered_image(resource_key: string, resource_description: wbe.RDParamsImageVK): graph.Resource
    local registration = make_registration(resource_key, "RDParamsImageVK", resource_description)
    local resource = graph.import(resource_key)
    resource.registration = registration
    return resource
end

-- A port of a subgraph, which the subgraph binds to a resource of the enclosing graph.
function graph.port(port_name: string): graph.Resource
    validate_name(port_name, "a port name")
    return new_resource({ kind = "port", key = port_name })
end

-- A dispatch of a compute_sequential task; its bindings map the pipeline's reflected names to resource handles.
function graph.indirect_dispatch(pipeline_handle: graph.Resource, indirect_buffer: graph.Resource, offset: integer,
    resource_bindings?: {string:graph.Resource}): graph.RTParamsComputeDispatchVK
    return { pipeline = pipeline_handle, indirect_buffer = indirect_buffer, indirect_offset = offset, bindings = resource_bindings }
end

-- A task of a registered type without a generated constructor, whose params name no resource.
function graph.task(type_name: string, parameters: any, settings?: graph.TaskOptions): graph.Task
    validate_name(type_name, "a task type")
    if contains_handle(parameters) then
        error("render_graph: task params of type '" .. type_name .. "' name resources; use its typed constructor.", 2)
    end
    local options: graph.TaskOptions = settings or {}
    local trigger, toggle = gating_options(options)
    return new_task({
        kind = "typed",
        name = options.name,
        type = type_name,
        params = snapshot(parameters),
        bindings = copy_bindings(options.bindings),
        trigger = trigger,
        toggle = toggle,
    })
end

-- A task C++ supplies under identifier (RendGraphRecipeImportsVK::add_task()); its bindings map the identifiers it uses
-- to resources.
function graph.import_task(identifier: string): graph.Task
    validate_name(identifier, "an imported task id")
    return new_task({ kind = "import", key = identifier })
end

-- A task that records the selected one of its branches: typed tasks without edges, triggers, or toggles. The branches
-- share the multiplexer's bindings, and must run on one queue family.
function graph.multiplex(descriptor: graph.MultiplexDescriptor): graph.Task
    validate_list(descriptor.branches, "branch", false)
    if descriptor.select == nil then
        error("render_graph: a multiplexer sets no select.", 2)
    end
    local branches: {graph.Task} = {}
    for index, branch in ipairs(descriptor.branches) do
        validate_task(branch)
        if branch.kind ~= "typed" then
            error("render_graph: multiplexer branch " .. index .. " is not a typed task.", 2)
        end
        if #branch.edges ~= 0 or branch.trigger ~= nil or branch.toggle ~= nil then
            error("render_graph: multiplexer branch " .. index .. " connects, triggers, or toggles; set those on the multiplexer.", 2)
        end
        branches[index] = branch
    end
    return new_task({
        kind = "multiplex",
        name = descriptor.name,
        branch_tasks = branches,
        select = snapshot(descriptor.select) as wbe.RendRecipeSelectVK,
        bindings = copy_bindings(descriptor.bindings),
        trigger = snapshot(descriptor.trigger) as wbe.RendRecipeTriggerVK,
        toggle = snapshot(descriptor.toggle) as wbe.RendRecipeToggleVK,
    })
end

-- The tasks of descriptor as one subgraph task, whose ports map each port name its tasks use to a resource of the
-- enclosing graph. Its trigger and toggle gate every member.
function graph.subgraph(descriptor: graph.SubgraphDescriptor): graph.Task
    validate_list(descriptor.tasks, "task", true)
    local subgraph_tasks: {graph.Task} = {}
    for index, task in ipairs(descriptor.tasks) do
        validate_task(task)
        subgraph_tasks[index] = task
    end
    return new_task({
        kind = "subgraph",
        name = descriptor.name,
        subgraph_tasks = subgraph_tasks,
        ports = copy_bindings(descriptor.ports),
        trigger = snapshot(descriptor.trigger) as wbe.RendRecipeTriggerVK,
        toggle = snapshot(descriptor.toggle) as wbe.RendRecipeToggleVK,
    })
end

-- Order tasks on every one of resources, each after the one before it.
local function chain(resources: any, tasks: {graph.Task})
    local list = connection_resources(resources)
    validate_list(tasks, "task", true)
    for _, task in ipairs(tasks) do
        validate_task(task)
    end
    for _, resource in ipairs(list) do
        for i = 2, #tasks do
            append_edge(tasks[i - 1], tasks[i], resource)
        end
    end
end
graph.chain = chain

local function non_empty<T>(map: {string:T}): {string:T}
    return next(map) ~= nil and map or nil
end

-- Emit the recipe of tasks: generate the ids of the declared resources and tasks they reach, in the order of the list.
-- port_names are the ports of a nested recipe, and exports the resources C++ acquires by name.
local function build_recipe(tasks: {graph.Task}, port_names: {string},
    exports: {string:graph.Resource}): wbe.RendGraphRecipeVK
    validate_list(tasks, "task", true)
    local resources: {wbe.RendRecipeResourceVK} = {}
    local imports: {string} = {}
    local ports: {string} = {}
    local import_ids: {string:boolean} = {}
    local port_ids: {string:boolean} = {}
    local ids: {graph.Resource:string} = {}
    local generated_ids: {string:boolean} = {}
    local ordinals: {string:integer} = {}
    local visiting: {graph.Resource:boolean} = {}
    local wire_registrations: {wbe.RendRecipeRegistrationVK} = {}
    local registrations_by_key: {string:wbe.RendRecipeRegistrationVK} = {}

    local function add_registration(registration: wbe.RendRecipeRegistrationVK)
        local previous = registrations_by_key[registration.key]
        if previous == nil then
            registrations_by_key[registration.key] = registration
            wire_registrations[#wire_registrations + 1] = registration
        elseif previous.type ~= registration.type or not deep_equal(previous.description, registration.description) then
            error("render_graph: resource '" .. registration.key .. "' is registered with different descriptions.", 4)
        end
    end

    -- `<kind>-<hash>` of a kind, a debug name, and how many ids of that kind and name were generated before it, so that
    -- rebuilds, which reach everything in the same order, keep every id, and a named one keeps its id when unrelated
    -- declarations are added.
    local function generate_id(kind: string, name: string): string
        local ordinal_key = kind .. "\0" .. (name or "")
        local ordinal = (ordinals[ordinal_key] or 0) + 1
        ordinals[ordinal_key] = ordinal
        local id = string.format("%s-%08x", kind, hash(ordinal_key .. "\0" .. ordinal))
        if generated_ids[id] then
            error("render_graph: generated id '" .. id .. "' collides; rename '" .. tostring(name) .. "'.", 4)
        end
        generated_ids[id] = true
        return id
    end

    local function add_import(identifier: string)
        if port_ids[identifier] then
            error("render_graph: '" .. identifier .. "' is both an import and a port.", 4)
        end
        if not import_ids[identifier] then
            imports[#imports + 1] = identifier
            import_ids[identifier] = true
        end
    end

    local function add_port(port_name: string)
        if import_ids[port_name] then
            error("render_graph: '" .. port_name .. "' is both an import and a port.", 4)
        end
        if not port_ids[port_name] then
            ports[#ports + 1] = port_name
            port_ids[port_name] = true
        end
    end

    for _, port_name in ipairs(port_names) do
        add_port(port_name)
    end

    local function resource_id(resource_handle: graph.Resource): string
        validate_graph_resource(resource_handle)
        local id = ids[resource_handle]
        if id ~= nil then
            return id
        end
        if visiting[resource_handle] then
            error("render_graph: resource dependency cycle.", 4)
        end
        visiting[resource_handle] = true
        local dependency_ids: {string} = {}
        for _, dependency in ipairs(resource_handle.dependencies) do
            dependency_ids[#dependency_ids + 1] = resource_id(dependency)
        end
        visiting[resource_handle] = nil
        local kind = resource_handle.kind
        if kind == "import" then
            id = resource_handle.key
            add_import(id)
            if resource_handle.registration ~= nil then
                add_registration(resource_handle.registration)
            end
        elseif kind == "port" then
            id = resource_handle.key
            add_port(id)
        elseif kind == "member" then
            id = resource_handle.key
        else
            id = generate_id(kind, resource_handle.name)
            local description = snapshot(resource_handle.description) as wbe.RendRecipeResourceVK
            description.id = id
            description.name = resource_handle.name
            if description.image_view ~= nil then
                description.image_view.image = dependency_ids[1]
            end
            resources[#resources + 1] = description
        end
        ids[resource_handle] = id
        return id
    end

    local function lower_bindings(resource_bindings: {string:graph.Resource}, aliases: {string:string})
        for _, local_name in ipairs(sorted_keys(resource_bindings or {})) do
            local id = resource_id(resource_bindings[local_name])
            local previous = aliases[local_name]
            if previous ~= nil and previous ~= id then
                error("render_graph: task-local resource '" .. local_name .. "' is bound to two resources.", 4)
            end
            aliases[local_name] = id
        end
    end

    -- Lower a typed task's params: a graph handle becomes its id, which the task binds to itself, and a registered handle
    -- its key, whose registration the recipe emits.
    local function lower_params(parameters: any, fields: {render_graph_tasks.ResourceField}, aliases: {string:string}): any
        if fields == nil then
            return parameters
        end
        local function lower(input_value: any, resource_field: render_graph_tasks.ResourceField): any
            if input_value == nil then
                return nil
            elseif resource_field.kind == "resource" then
                local resource = input_value as graph.Resource
                if resource_field.scope == "registered" then
                    if resource.registration ~= nil then
                        add_registration(resource.registration)
                    end
                    return resource.key
                end
                local id = resource_id(resource)
                aliases[id] = id
                return id
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
            local result: {string:any} = {}
            for key, value in pairs(values) do
                result[key] = value
            end
            for _, field in ipairs(resource_field.fields) do
                result[field.name] = lower(values[field.name], field)
            end
            return result
        end
        local result = lower(parameters, { kind = "record", fields = fields })
        if contains_handle(result) then
            error("render_graph: task params hold a handle in a field that names no resource.", 4)
        end
        return result
    end

    -- Task ids first, so that edges may point to later tasks.
    local task_ids: {graph.Task:string} = {}
    for index, task in ipairs(tasks) do
        validate_task(task)
        if task_ids[task] ~= nil then
            error("render_graph: task " .. index .. " is listed twice.", 3)
        end
        task_ids[task] = task.kind == "import" and task.key or generate_id(task.type or task.kind, task.name)
    end
    local wire_tasks: {wbe.RendRecipeTaskVK} = {}
    for _, task in ipairs(tasks) do
        local aliases: {string:string} = {}
        local wire_task: wbe.RendRecipeTaskVK = {
            id = task_ids[task],
            name = task.name,
            type = task.type,
            import = task.kind == "import" or nil,
            trigger = task.trigger,
            toggle = task.toggle,
            select = task.select,
        }
        if task.kind == "typed" then
            wire_task.params = lower_params(task.params, task.fields as {render_graph_tasks.ResourceField}, aliases)
        elseif task.kind == "multiplex" then
            local branches: {wbe.RendRecipeBranchVK} = {}
            for _, branch in ipairs(task.branch_tasks) do
                lower_bindings(branch.bindings, aliases)
                branches[#branches + 1] = {
                    type = branch.type,
                    params = lower_params(branch.params, branch.fields as {render_graph_tasks.ResourceField}, aliases),
                }
            end
            wire_task.branches = branches
        elseif task.kind == "subgraph" then
            wire_task.subgraph = build_recipe(task.subgraph_tasks, sorted_keys(task.ports), {})
            local wire_ports: {string:string} = {}
            lower_bindings(task.ports, wire_ports)
            wire_task.ports = wire_ports
        end
        lower_bindings(task.bindings, aliases)
        wire_task.aliases = non_empty(aliases)
        if #task.edges ~= 0 then
            local edges: {wbe.RendRecipeEdgeVK} = {}
            for _, edge in ipairs(task.edges) do
                local to = task_ids[edge.to]
                if to == nil then
                    error("render_graph: an edge leaves the built tasks; list both of its tasks.", 3)
                end
                edges[#edges + 1] = { to = to, resource = resource_id(edge.resource) }
            end
            wire_task.connect = edges
        end
        wire_tasks[#wire_tasks + 1] = wire_task
    end
    local wire_exports: {string:string} = {}
    for _, export_name in ipairs(sorted_keys(exports)) do
        validate_name(export_name, "an export name")
        wire_exports[export_name] = resource_id(exports[export_name])
    end
    return { imports = imports, registrations = wire_registrations, resources = resources, tasks = wire_tasks,
        ports = ports, exports = non_empty(wire_exports) }
end

-- The recipe of descriptor's tasks, in order, and its exports.
function graph.build(descriptor: graph.BuildDescriptor): wbe.RendGraphRecipeVK
    local exports: {string:graph.Resource} = {}
    for export_name, resource in pairs(descriptor.exports or {}) do
        validate_graph_resource(resource)
        exports[export_name] = resource
    end
    return build_recipe(descriptor.tasks, {}, exports)
end

return graph
