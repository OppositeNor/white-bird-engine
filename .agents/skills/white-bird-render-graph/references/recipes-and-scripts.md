# Render Graph Recipes and Scripts

## Flow

1. A `render_graph` resource (`RURenderGraph`) names the recipe and its static `parameters`. For each build of an instance,
   its bindings' `prepare()` returns the serialized context and a `RendGraphRecipeImportsVK` with every resource
   (`add(name, bindable)`) and native task (`add_task(RendTaskDescVK)`) owned outside the graph; the graph adds the swap
   chain when the recipe imports it and the bindings do not.
2. `RendGraphScriptVK::run(script, context, parameters)` runs the recipe in a fresh sandboxed `SBLua` and returns its table
   as a `DataValue`. `require` resolves other script resources through `SMRResourceManager`.
3. `RendGraphRecipeBuilderVK().build(recipe, imports, render_context, &graph)` returns a `RendGraphDescVK` bound to the
   graph. It first applies the recipe's `registrations` to the graph, then adds tasks through
   `RenderTaskRegistryVK::add_task()` (which runs the type's `prepare_graph`), wraps a `trigger` first and a `toggle`
   outermost, copies imported tasks, and builds subgraphs. A declared-but-missing or supplied-but-undeclared import or task
   is an error.
4. `RenderGraphVK::build_from_recipe()` runs these steps for the first build and every rebuild. Nothing is added to the
   descriptor in C++: whatever the application owns and a shader names (the deferred test's acceleration structure and
   dynamic mesh entities) is a declared import.

Recipes only build descriptors; they never touch render objects.

## The graph API

`local graph = require("render_graph_utils.render_graph_utils")` gives the implementation. There is no graph object: the
module's functions make free-standing resource and task handles, and `graph.build()` emits what the listed tasks reach.
The module also exposes the types `graph.Resource`, `graph.Task`, the descriptor interfaces (`graph.BufferDescriptor`,
`graph.ImageDescriptor`, `graph.ImageViewDescriptor`, `graph.SamplerDescriptor`), and the handle-accepting params
interfaces `graph.RTParams<Name>VK`. For types only, import the generated declarations with
`local type graph = require("render_graph_utils.render_graph")`.

### Resources

| Call | Returns |
| --- | --- |
| `graph.buffer{ name?, usage, size, ... }`, `graph.storage_buffer{ name?, size, ... }` | Graph-owned buffer (`RendBufferParamsVK`); `storage_buffer` sets usage `buffer_storage` |
| `graph.image{ name?, usage, format, extent?, ... }`, `graph.storage_image{ name?, format, extent?, ... }` | Graph-owned image (`RendImageParamsVK`; an omitted extent follows the swap chain); `storage_image` sets usage `image_read_write` |
| `image:view()`, `image:view(descriptor)`, `graph.image_view(image, descriptor?)` | View; `:view()` caches the default view, which takes the image's name; a descriptor (`RendImageViewParamsVK` plus `name`) selects a mip, layer, or aspect range |
| `graph.clamp_to_edge_sampler{ name?, ... }` | Graph-owned sampler (`RendSamplerParamsVK`) |
| `graph.import(id)` | A resource C++ supplies under `id` through `RendGraphRecipeImportsVK`; handles of one id denote one import |
| `import:member(name)` | The member `<id>.<name>` of an imported composite (a mesh entity's transform, an acceleration structure's `top_level`) |
| `graph.port(name)` | A port of a subgraph, bound by `graph.subgraph{ ports = ... }` |
| `graph.mesh(key, description?)`, `graph.compute_pipeline(key, description?)`, `graph.graphics_pipeline(key, description?)` | Entry registered on the graph; with a description the handle carries its registration, without one it names an entry registered elsewhere. Usable only in task params, never as an edge or binding |
| `graph.shader_compute_pipeline(shader)` | A compute pipeline compiled from the shader of that key and registered under it |
| `graph.registered_image(key, description)` | Registers a texture and returns an import handle; the builder binds the registered view |
| `graph.registered_resource(key, type, description, resource_type)` | Registers a description of a custom reflected type `type`, which selects its `IResourceRegistrator` |

Declared resources get generated ids, `<kind>-<8 hex>`, from their kind, debug name, and ordinal among declarations of that
kind and name in the order the build reaches them, so rebuilds keep every id (and content) and a named resource keeps its id
when others are added. The name is the Vulkan debug name of the graph's objects (`<graph key>.<name>`) and appears in
diagnostics; name every resource and never read the ids. A registration is emitted when a built task's params reach its
handle; equal registrations of a key merge and different ones fail.

```lua
local shadow_depth = graph.image({
    name = "shadowmap_depth",
    usage = "image_depth_target",
    format = context.shadow_depth_format,
    extent = shadow_extent,
}):view()
local mask = graph.storage_image({ name = "pixel_sort_mask", format = "r16_uint", extent = extent }):view()
local transform = graph.import(entity_id):member(context.dynamic_mesh_transform_member)
```

### Tasks

The typed constructors are generated from task classes annotated `WBE_RENDER_TASK(<method>)` (see the table in
[system-and-usage.md](system-and-usage.md#built-in-task-types)). Each is `graph.<method>(params, options?)`, where
`options` is `{ name?, bindings?, trigger?, toggle? }`, and returns a `graph.Task`. Params type-check against
`graph.RTParams<Name>VK`, which accepts handles, and only handles, in the fields marked `WBE_RECIPE_RESOURCE`. Params and
option tables are copied, so a table can be reused for several tasks.

```lua
local write_pixels = graph.compute({
    pipeline = graph.shader_compute_pipeline("write_sorted_pixels_" .. orientation .. "_shader"),
    group_count_x = tile_group_count_x,
    group_count_y = tile_group_count_y,
}, { name = "write_sorted_pixels", bindings = {
    pixel_sort_source = source,
    pixel_sort_keys = sort_buffers.keys,
    pixel_sort_mask = mask,
    pixel_sort_destination = destination,
} })
```

- `name` is the task's debug name (`RendTaskDescVK::debug_name`), which profiles and diagnostics show; the build generates
  task ids, `<type>-<8 hex>` like resource ids.
- Every name a task's shaders reflect or its render targets use is bound explicitly, imports included. Recipe tasks' maps
  are strict (`RenderTaskResourceMap::require_bindings()`), so an unbound name fails; nothing matches a resource by id. A
  handle in a params resource field is bound by the build.
- `task:bind(name, resource)` adds a binding; a name binds once per task, and different tasks may reuse it.
- `task:color_target(index, resource)` and `task:depth_target(resource)` bind `render_target_colors[<index>]` and
  `render_target_depth`.
- A `compute_sequential` dispatch's `bindings` (and `graph.indirect_dispatch(pipeline, buffer, offset, bindings?)`) map the
  names of that dispatch's pipeline to handles, so dispatches of one task can bind one shader name differently (the radix
  sort's passes alternate between its key buffers). Dispatch bindings belong to one dispatch; task bindings to the task.
- `graph.task(type, params, options?)` creates a registered type without a constructor; its params name no resource.
- `graph.import_task(id)` references a C++ task supplied with `imports.add_task()`.

Task methods return the task, so they chain fluently.

### Edges

Each function below adds one edge per resource between task handles; a resource argument is one handle or a non-empty
list of them.

- `graph.chain(resources, { a, b, c })` orders adjacent tasks: `a -> b -> c`. Every adjacent pair must access every resource.
- `a:connect(b, resources)` is a two-task chain.
- `a:connect_all({ b, c }, resources)` fans out, leaving `b` and `c` unordered with each other.
- `b:inputs_from(a, { local_name = resource, ... })` binds each name on `b` and adds one `a -> b` edge per distinct
  resource.

Both tasks of every edge must be in the built task list.

### Building and the emitted schema

`graph.build({ tasks = { ... }, exports = { <name> = resource } })` takes the tasks in order, which fixes their ids, and
lowers everything to the plain `RendGraphRecipeVK` tables C++ reads (`rend_graph_recipe_params_vk.hh`); handles never cross
the Lua/C++ boundary. A task listed twice, or an edge to an unlisted task, fails.

- `imports`: every import id the tasks reach.
- `registrations`: `{ key, type, description }`, where `type` is the reflected name of the description struct, resolved
  through `TypeUUIDManager`; applied before that level's imports and tasks.
- `resources`: each has an `id`, an optional `name`, and exactly one of `buffer`, `image`, `image_view` (`{ image, view }`),
  or `sampler`.
- `tasks`: `{ id, name, type, params, aliases, connect = { { to, resource } }, trigger, toggle, branches, select, subgraph,
  ports }`, where `aliases` is the task's bindings (the C++ `RendTaskDescVK::alias()` map). Imported tasks carry `id`,
  `import = true`, `aliases`, and `connect`.
- `exports`: export names to resource ids, which `RenderGraphVK::acquire_resource(<export name>)` takes, so C++ reaches a
  resource whose generated id it does not know. Only the outermost recipe exports.

Params are strictly deserialized: unknown keys fail. Enums serialize as canonical names; compare
`graph.enum_name(value) == "CANONICAL_NAME"`, which also accepts the lowercase aliases.

## Context

`RendRecipeContextVK` holds `frames_in_flight`, `swap_chain_format`, and `swap_chain_extent`; the graph fills these
authoritative renderer facts itself. An application that adds entries declares a `WBE_REFLECT_SCRIPT` struct deriving
from it, assigns the base from the facts its bindings receive, and returns `serialize_context(context)` from `prepare()`;
the recipe annotates `build(context: wbe.<ContextType>, parameters: wbe.<ParametersType>)` (or `{string:any}` for
unused parameters). Static choices that distinguish definitions of one recipe belong in the definition's `parameters`
(`DeferredRecipeParameters`); runtime state in the context. Size swap-chain-dependent resources and dispatches in Lua from `swap_chain_extent`.
Pass runtime state that rebuilds must preserve (toggle states, feature parameters) through the context.

Examples: `tests/executable/function/render/pixel_sorting_recipe_context.hh`, `shadowed_mesh_recipe_context.hh`,
`deferred_test/deferred_recipe_context.hh`.

## Triggers and toggles

- `trigger = { event, activate }` wraps the task with `RTTriggeredVK`, which records its child while it has remaining
  frames. The application emits `RTTriggeredVK::Command::trigger()` or `enable_for(n)` through `EventManager::emit(event,
  command)`. Run-once work (a static build) is a trigger activated while still pending; the application emits
  `enable_for(0)` after the first recorded frame, so later rebuilds create it inactive.
- `toggle = { event, enabled }` wraps it with `RTToggleVK`, outside any trigger, recording every frame while enabled
  (`Command::enable()` / `disable()`). Rebuilds re-run the recipe, so pass the toggle's current state through the context.
- A trigger or toggle on a subgraph task wraps every member; set it in `graph.subgraph{ trigger?, toggle? }`.
- A multiplexer records one of several branch tasks: build them with the typed constructors, without edges, triggers, or
  toggles, and pass them to `graph.multiplex{ name?, branches = { ... }, select = { event, branch }, bindings?, trigger?,
  toggle? }`, emitted as `branches = { { type, params } }` and `select`. `RTMultiplexerVK` is one graph node: it reports and binds every branch,
  records the selected one, and switches on `RTMultiplexerVK::Command { branch }`; an out-of-range branch throws. Branches
  share one resource map (identical bindings merge, conflicting ones throw) and must run on one queue family, and they set
  no edges, triggers, or toggles of their own. Pass the current branch through the context.

`EventManager` lives in `function/event/event_manager.hh`; CPU readback tasks (`graph.cpu_acquire_image()`) also emit
their results through it under their params' explicit `event` id:

```lua
local acquire_params: graph.RTParamsCPUInFlightAcquireVK = { image_view = bloom_ping, event = context.bloom_image_event }
local acquire_bloom = graph.cpu_acquire_image(acquire_params, {
    name = "acquire_bloom",
    trigger = { event = context.bloom_acquire_event },
})
```

## Imported C++ tasks

Build a `RendTaskDescVK` with its factory, supply it with `imports.add_task()`, and reference it in the recipe with
`graph.import_task(id)`; connect it like any task. An imported task keeps its id and sets no `type`, `params`, `trigger`, or
`toggle`; `:bind()` aliases the identifiers the C++ task uses to recipe resources (so the resources are reached and
imported), and what it leaves unmapped resolves to itself:

```lua
local build_acceleration_structure = graph.import_task("build_mesh_acceleration_structure")
    :bind("top_level_acceleration_structure", graph.import("mesh_acceleration_structure"):member("top_level"))
local draw_imgui = graph.import_task("draw_imgui")
```
 Examples: `tests/executable/utilities/rt_imgui_vk.hh`, `deferred_render_graph.cpp`,
`vulkan_pixel_sorting_test.cpp`.

## Feature subgraph modules

A reusable recipe part lives in `render_graph_utils/features/` and exports `subgraph(name, params, target)`. It names its
ports with `graph.port(<port>)`, declares its own resources and tasks, and returns
`graph.subgraph{ name = name, tasks = { ... }, ports = { <port> = <enclosing resource> }, trigger?, toggle? }`, a task the
recipe lists in its build and connects like any task. The port map binds every port to an enclosing resource; tasks inside
reach the enclosing graph's declared resources only through ports, owned resources stay scoped to the subgraph, and debug
names become `<subgraph name>.<name>`.

```lua
local color = graph.port("color")
-- ... tasks bound to color and the feature's own resources, ordered with graph.chain() ...
return graph.subgraph({
    name = name,
    tasks = { copy_source, count_mask, prefix_sum, register_keys, radix_sort, write_pixels },
    ports = { color = target.color_view },
    toggle = { event = params.toggle_event, enabled = params.enabled },
})
```

Its C++ side provides `configure_recipe(DataValue&, RendGraphRecipeImportsVK&)`, which serializes its
`WBE_REFLECT_SCRIPT` params struct (derived from `RecipeFeatureParams`, including the `module` name) into the context and
supplies its resources and C++ tasks. The recipe calls `require(params.module).subgraph(name, params, target)`.

Examples: `features/pixel_sort.tl` with `deferred_test/pixel_sort.{hh,cpp}`, `features/voxel_svo.tl` and
`features/voxel_octree.tl` with the deferred test's voxel features, and `render_graphs/deferred.tl`.

## Adding a task type

1. Write `include/function/render_vk/rt_params_<name>_vk.hh` with
   `struct WBE_META(WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT) RTParams<Name>VK`; mark required
   fields `WBE_REFLECT_REQUIRED`. Mark fields that hold ids so the constructor takes handles (only handles) there:
   a bare `WBE_RECIPE_RESOURCE` for graph resources, which the build binds to themselves, and
   `WBE_RECIPE_RESOURCE(registered, <RenderObjectType>)` for keys registered on the graph such as pipelines and meshes (the
   type must be a `WBE_RENDER_OBJECT` class). Values that are not resources, such as event ids, stay plain fields. Such a field is a `std::string`, optional or in a vector, or a `DataValue` map.
2. Mark the task `WBE_META(WBE_RENDER_TASK(<method>), WBE_SERIALIZATION_ALIASES(<name>))`, or with a bare
   `WBE_RENDER_TASK` to register it without a recipe constructor, and declare
   `WBE_META(WBE_REFLECT) static Unique<IRenderTaskVK> create(const RenderContextVK*, const RTParams<Name>VK&, const RendGraphContextVK&)`,
   or `create(const RenderContextVK*, IRenderGraph*, const RTParams<Name>VK&, const RendGraphContextVK&)` when it resolves
   registered resources such as pipelines.
   Add `static void prepare_graph(RendGraphDescVK&, RendTaskDescVK&, const RTParams<Name>VK&, const RenderContextVK*)` if
   the task must add imports or aliases.
3. Use `render_task_create_utils_vk.hh` (`acquire_task_compute_pipeline_vk`, `acquire_task_graphics_pipeline_vk`,
   `resolve_task_push_constants_vk`) in `create()`.
4. Rebuild. `render_task_registry.gen.cpp` registers the class name and aliases; a missing `create` or duplicate name fails
   the build. The params struct appears in `wbe.d.tl`, its handle-accepting interface in the generated
   `render_graph_utils.render_graph`, and the `graph.<method>()` constructor, which creates the type `<name>`, in the
   generated `render_graph_utils.render_graph_tasks`.

Examples: `rt_full_memory_barrier_vk.hh` (simple), `rt_run_compute_pipeline_vk.hh` (standard), `rt_graphics_draw_mesh_vk.hh`
(with `prepare_graph`), and `tests/unit/function/render_task_registry_vk_test_tasks.hh` (test probes).

## Scripts and Teal

- Sources live under `res/assets/scripts/teal/` and are `script` resources declared in `lua_scripts.toml`: recipes in
  `render_graphs/` (`deferred.tl`, `draw_cube_unshaded.tl`, `pixel_sorting.tl`), features in `render_graph_utils/features/`,
  graph helpers in `render_graph_utils/` (`render_graph_utils.tl`, `sort.tl`, `large_prefix_scan.tl`, `dedup_large.tl`,
  `svo_3.tl`), and general helpers in `utils/` (`math.tl`). Test-only scripts under `tests/` use `scope = "test_only"`.
- A script's resource id is its module name: path under `scripts/teal/` without extension, dots for slashes. Keep them in
  sync so `require` resolves the same module at runtime and in the editor.
- Reflection generates `render_graph_utils.render_graph` (declarations) and `render_graph_utils.render_graph_tasks`
  (constructor installer) into `build/<target>/teal/`; their manifest entries give `source_file` relative to that directory.
- `ACPCompilerScript` copies `.lua` sources and type-checks and compiles `.tl` sources with the vendored compiler; a type
  error fails the build. A Teal module requires a Lua module only through a `.d.tl` declaration next to it.
- Import engine types as types only (`local type wbe = require("wbe")`); declare data C++ hands a recipe as
  `WBE_REFLECT_SCRIPT` structs rather than Teal records.
- `require` is the only import mechanism; modules load once per backend and cyclic requires fail. New import sources are
  `IScriptModuleResolver`s whose `try_resolve()` must not throw.

`render_graph_utils.sort` provides `radix_sort_buffers({ name?, max_element_count, key_type? })`, which declares the
sort's buffers (named `<name>.keys` and so on), and `radix_sort_task(buffers, { name?, queue_family?, begin_bit?, end_bit?
})`, for `uint` keys or `uint2` keys ordered by `x` then `y`, sorted by every bit or by a bit range of an even number of
8-bit digits (bits `[32, 64)` of `uint2` keys sort by `x` and carry `y` as a payload, which
`radix_sort_key_value_task(buffers, options?)` does); `radix_sort_dispatches(buffers, bits?)` returns the passes for a
longer `compute_sequential` task. `large_prefix_scan` and `dedup_large` likewise provide `buffers()` and `dispatches()`;
`utils.math` provides `ceil_div` and `clamp`.

## Tests

- `tests/unit/function/rend_graph_recipe_builder_vk_test.cpp`: imports, tasks, subgraphs, triggers, toggles.
- `rend_graph_script_vk_test.cpp`: runs `tests.teal_recipe`, `tests.teal_graph_api`, and the shipped recipes with their
  features.
- `render_task_registry_vk_test.cpp`, `render_task_registry_vk_builtin_test.cpp`: registration, params, push constants.
- `rt_triggered_vk_test.cpp`, `rt_toggle_vk_test.cpp`, `rt_graphics_draw_mesh_vk_test.cpp`, `teal_declarations_test.cpp`.
- `tests/build_script/reflection/test_teal_render_tasks.py`: constructor and params-interface generation.
