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

    -- Resource-bearing task parameters mirror the reflected C++ descriptions, replacing resource IDs with handles.
    -- String IDs remain accepted for existing recipes and task-local names in sequential dispatches.
    record DrawMeshParams
        mesh: string
        pipeline: string
        parts: {wbe.MeshPartVK | integer}
        transform: Resource | string
        render_extent: wbe.RendExtent2DParamsVK
        clear_values: {wbe.RTParamsClearValueVK}
        color_load_op: wbe.AttachmentLoadOpVK | integer
        depth_load_op: wbe.AttachmentLoadOpVK | integer
    end

    record DrawIndirectParams
        pipeline: string
        index_buffer: Resource | string
        indirect_buffer: Resource | string
        draw_count: integer
        render_extent: wbe.RendExtent2DParamsVK
        clear_values: {wbe.RTParamsClearValueVK}
        color_load_op: wbe.AttachmentLoadOpVK | integer
        depth_load_op: wbe.AttachmentLoadOpVK | integer
    end

    record ComputeIndirectParams
        pipeline: string
        indirect_buffer: Resource | string
        indirect_offset: integer
        queue_family: wbe.QueueFamilyVK | integer
        push_constants: any
    end

    record ComputeDispatchParams
        pipeline: string
        indirect_buffer: Resource | string
        indirect_offset: integer
        aliases: {string:Resource | string}
        group_count_x: integer
        group_count_y: integer
        group_count_z: integer
        push_constants: any
    end

    record ComputeSequentialParams
        dispatches: {ComputeDispatchParams}
        queue_family: wbe.QueueFamilyVK | integer
    end

    record GenerateMipmapsParams
        source_view: Resource | string
        mip_chain_view: Resource | string
        base_extent: wbe.RendExtent2DParamsVK
        mipmap_level_count: integer
        format: wbe.ImageFormatVK | integer
    end

    record CPUAcquireImageParams
        image_view: Resource | string
    end

    record FullMemoryBarrierParams
        resource: Resource | string
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

    record Graph
        tasks: {Task}
        add: function(self: Graph, p_task: Task): Task
        get: function(self: Graph, p_id: string): Task
        compute: function(self: Graph, p_id: string, p_params: wbe.RTParamsRunComputePipelineVK,
            p_options?: TaskOptions): Task
        draw: function(self: Graph, p_id: string, p_params: wbe.RTParamsRunGraphicsPipelineVK,
            p_options?: TaskOptions): Task
        draw_mesh: function(self: Graph, p_id: string, p_params: DrawMeshParams, p_options?: TaskOptions): Task
        draw_indirect: function(self: Graph, p_id: string, p_params: DrawIndirectParams, p_options?: TaskOptions): Task
        compute_indirect: function(self: Graph, p_id: string, p_params: ComputeIndirectParams,
            p_options?: TaskOptions): Task
        compute_sequential: function(self: Graph, p_id: string, p_params: ComputeSequentialParams,
            p_options?: TaskOptions): Task
        generate_mipmaps: function(self: Graph, p_id: string, p_params: GenerateMipmapsParams,
            p_options?: TaskOptions): Task
        cpu_acquire_image: function(self: Graph, p_id: string, p_params: CPUAcquireImageParams,
            p_options?: TaskOptions): Task
        full_memory_barrier: function(self: Graph, p_id: string, p_params: FullMemoryBarrierParams,
            p_options?: TaskOptions): Task
        ping_pong: function(self: Graph, p_id: string, p_params: wbe.RTParamsPingPongVK,
            p_options?: TaskOptions): Task
        import_task: function(self: Graph, p_id: string): Task
        chain: function(self: Graph, p_resource: Resource, p_tasks: {Task})
        build: function(self: Graph, p_imports?: {string}, p_ports?: {string}): wbe.RendGraphRecipeVK
    end
end

local function validate_id(p_id: string)
    if type(p_id) ~= "string" or p_id == "" then
        error("render_graph: resource and task ids must be non-empty strings.", 3)
    end
end

local function attach_task(p_task: render_graph.Task,
    p_validate_task?: function(p_task: render_graph.Task, p_local_name?: string,
        p_resource?: render_graph.Resource)): render_graph.Task
    function p_task:alias(p_local_name: string, p_resource: render_graph.Resource): render_graph.Task
        if p_validate_task == nil then
            error("render_graph: add the task to a graph before assigning an alias.", 2)
        end
        validate_id(p_local_name)
        validate_id(p_resource.id)
        p_validate_task(self, p_local_name, p_resource)
        self.aliases = self.aliases or {}
        if self.aliases[p_local_name] ~= nil then
            error("render_graph: task-local resource '" .. p_local_name .. "' is mapped more than once.", 2)
        end
        self.aliases[p_local_name] = p_resource
        return self
    end

    function p_task:connect(p_next_task: render_graph.Task, p_resource: render_graph.Resource): render_graph.Task
        if p_validate_task == nil then
            error("render_graph: add the task to a graph before connecting it.", 2)
        end
        p_validate_task(self)
        self.graph:chain(p_resource, { self, p_next_task })
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

local function sorted_keys<T>(p_map: {string:T}): {string}
    local keys: {string} = {}
    for key in pairs(p_map) do
        keys[#keys + 1] = key
    end
    table.sort(keys)
    return keys
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

    function graph:compute(p_id: string, p_params: wbe.RTParamsRunComputePipelineVK,
        p_options?: render_graph.TaskOptions): render_graph.Task
        return make_task(p_id, "run_compute_pipeline", p_params, p_options)
    end

    function graph:draw(p_id: string, p_params: wbe.RTParamsRunGraphicsPipelineVK,
        p_options?: render_graph.TaskOptions): render_graph.Task
        return make_task(p_id, "run_graphics_pipeline", p_params, p_options)
    end

    function graph:draw_mesh(p_id: string, p_params: render_graph.DrawMeshParams,
        p_options?: render_graph.TaskOptions): render_graph.Task
        local uses: {render_graph.Resource} = {}
        local params = copy_fields(p_params)
        params.transform = resource_id(p_params.transform, uses)
        return make_task(p_id, "graphics_draw_mesh", params, p_options, uses)
    end

    function graph:draw_indirect(p_id: string, p_params: render_graph.DrawIndirectParams,
        p_options?: render_graph.TaskOptions): render_graph.Task
        local uses: {render_graph.Resource} = {}
        local params = copy_fields(p_params)
        params.index_buffer = resource_id(p_params.index_buffer, uses)
        params.indirect_buffer = resource_id(p_params.indirect_buffer, uses)
        return make_task(p_id, "run_graphics_pipeline_indirect", params, p_options, uses)
    end

    function graph:compute_indirect(p_id: string, p_params: render_graph.ComputeIndirectParams,
        p_options?: render_graph.TaskOptions): render_graph.Task
        local uses: {render_graph.Resource} = {}
        local params = copy_fields(p_params)
        params.indirect_buffer = resource_id(p_params.indirect_buffer, uses)
        return make_task(p_id, "run_compute_pipeline_indirect", params, p_options, uses)
    end

    function graph:generate_mipmaps(p_id: string, p_params: render_graph.GenerateMipmapsParams,
        p_options?: render_graph.TaskOptions): render_graph.Task
        local uses: {render_graph.Resource} = {}
        local params = copy_fields(p_params)
        params.source_view = resource_id(p_params.source_view, uses)
        params.mip_chain_view = resource_id(p_params.mip_chain_view, uses)
        return make_task(p_id, "generate_mipmaps", params, p_options, uses)
    end

    function graph:cpu_acquire_image(p_id: string, p_params: render_graph.CPUAcquireImageParams,
        p_options?: render_graph.TaskOptions): render_graph.Task
        local uses: {render_graph.Resource} = {}
        local params = copy_fields(p_params)
        params.image_view = resource_id(p_params.image_view, uses)
        return make_task(p_id, "cpu_acquire_image", params, p_options, uses)
    end

    function graph:full_memory_barrier(p_id: string, p_params: render_graph.FullMemoryBarrierParams,
        p_options?: render_graph.TaskOptions): render_graph.Task
        local uses: {render_graph.Resource} = {}
        local params = copy_fields(p_params)
        params.resource = resource_id(p_params.resource, uses)
        return make_task(p_id, "full_memory_barrier", params, p_options, uses)
    end

    function graph:ping_pong(p_id: string, p_params: wbe.RTParamsPingPongVK,
        p_options?: render_graph.TaskOptions): render_graph.Task
        return make_task(p_id, "ping_pong", p_params, p_options)
    end

    function graph:import_task(p_id: string): render_graph.Task
        return self:add({ id = p_id, import = true })
    end

    function graph:compute_sequential(p_id: string, p_params: render_graph.ComputeSequentialParams,
        p_options?: render_graph.TaskOptions): render_graph.Task
        local options: render_graph.TaskOptions = p_options or {}
        local aliases: {string:render_graph.Resource} = {}
        local uses: {render_graph.Resource} = {}
        for key, resource in pairs(options.aliases or {}) do
            aliases[key] = resource
        end
        -- A handle binds its graph ID as an identity alias. This cannot shadow an unrelated same-name shader binding.
        -- Keep every handle reachable so collection still rejects distinct owned descriptions of the same graph ID.
        local function local_id(p_value: render_graph.Resource | string): string
            if p_value == nil then
                return nil
            end
            if p_value is string then
                return p_value
            end
            local id = resource_id(p_value, uses)
            local previous = aliases[id]
            if previous ~= nil and previous.id ~= id then
                error("render_graph: sequential resource '" .. id .. "' conflicts with an explicit task-local alias.", 3)
            end
            if previous == nil then
                aliases[id] = p_value
            end
            return id
        end
        local dispatches: {wbe.RTParamsComputeDispatchVK} = {}
        for _, dispatch in ipairs(p_params.dispatches) do
            local wire_dispatch = copy_fields(dispatch)
            wire_dispatch.indirect_buffer = local_id(dispatch.indirect_buffer)
            if dispatch.aliases ~= nil then
                local dispatch_aliases: {string:string} = {}
                for _, key in ipairs(sorted_keys(dispatch.aliases)) do
                    dispatch_aliases[key] = local_id(dispatch.aliases[key])
                end
                wire_dispatch.aliases = dispatch_aliases
            end
            dispatches[#dispatches + 1] = wire_dispatch as wbe.RTParamsComputeDispatchVK
        end
        local params = copy_fields(p_params)
        params.dispatches = dispatches
        return make_task(p_id, "run_compute_pipelines_sequential", params, {
            aliases = next(aliases) ~= nil and aliases or options.aliases,
            uses = options.uses,
            trigger = options.trigger,
            toggle = options.toggle,
        }, uses)
    end

    function graph:get(p_id: string): render_graph.Task
        local task = task_by_id[p_id]
        if task == nil then
            error("render_graph: no task has the id '" .. tostring(p_id) .. "'.", 2)
        end
        validate_task(task)
        return task
    end

    function graph:chain(p_resource: render_graph.Resource, p_tasks: {render_graph.Task})
        for _, task in ipairs(p_tasks) do
            validate_task(task)
        end
        for i = 2, #p_tasks do
            local task = p_tasks[i - 1]
            task.edges = task.edges or {}
            task.edges[#task.edges + 1] = { to = p_tasks[i], resource = p_resource }
        end
    end

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
