---
name: white-bird-render-graph
description: Build, extend, and diagnose White Bird Engine's Vulkan render graphs and their Teal recipes. Apply when defining a render graph resource, acquiring or activating a graph instance and writing its bindings, writing or changing a render-graph recipe or feature subgraph module, declaring graph resources, imports, and task edges, adding or registering an IRenderTaskVK type and its RTParams struct, using triggers, toggles, imported C++ tasks, or subgraphs, choosing queues, or debugging render-graph synchronization, rebuild diffing, or lifecycle behavior.
---

# White Bird Render Graph

A render graph is a DAG of `IRenderTaskVK` nodes connected by resource dependency edges. Applications never build a graph
directly: a manifest **definition** (`type = "render_graph"`) names a Teal **recipe** and its static parameters, and the
application acquires an **instance** of it with **bindings** that supply runtime resources and native tasks. The graph
re-runs the recipe for its first build and every rebuild; applications never return or patch descriptors.

## Define, bind, and acquire a graph

```toml
# res/assets/render_graphs/render_graphs.toml (reachable from a root chunk)
[[resources]]
id = "my_graph"
type = "render_graph"
recipe = "render_graphs.my_graph"

[resources.parameters]
bloom_blur_pass_count = 8
```

```cpp
// The graph retains the bindings; their captures must not retain the graph and must outlive it.
auto bindings = make_ref<RenderGraphBindingsFunctionVK>(global_allocator(), "my_graph",
    [this](const RenderContextVK& /*p_render_context*/, const RendRecipeContextVK& p_facts) {
        RendGraphPreparedBindingsVK result;
        MyRecipeContext context; // a WBE_REFLECT_SCRIPT struct derived from RendRecipeContextVK, when the recipe needs one
        static_cast<RendRecipeContextVK&>(context) = p_facts;
        result.context = RendGraphScriptVK::serialize_context(context);
        result.imports.add("camera_info", *camera_info_ubo); // the renderer supplies "swap_chain" itself
        return result;
    });
Ref<IRenderGraph> graph = renderer->acquire_render_graph("my_graph", bindings); // on the rendering thread
renderer->activate_render_graph(graph);
// Before the renderer shuts down: renderer->activate_render_graph(MEM_NULL); graph = MEM_NULL;
```

- `prepare()` runs for the first build, when the context changes (swap-chain extent, checked at `execute()`), and on
  `rebuild()`; never on an unchanged frame. Keep pipeline compiles, application-owned buffers, and initial-data snapshots
  outside it; create the native task factories inside it, since every build recreates every task.
- The instance key identifies an instance of its definition: acquiring the same definition with the same instance key and
  bindings object returns the live graph, another bindings object for a live identity throws, and other instance keys are
  independent graphs (the pixel-sorting test keeps one per orientation). The renderer caches instances weakly; callers
  hold them.
- The graph replaces the renderer facts of the context with the authoritative ones and re-acquires the swap-chain
  container on every build.
- Rebuilds diff the new descriptor against the current graph. Matching graph-owned resources keep their content; changed ones
  are recreated in place or replaced. All tasks are recreated from their factories, so state that must survive a rebuild
  lives outside the task.

## Write the recipe

A recipe is a Teal `script` resource under `res/assets/scripts/teal/render_graphs/` whose `build(context, parameters)`
declares resource and task handles and returns `graph.build({ tasks = { ... } })`. There is no graph object:

```lua
local type wbe = require("wbe")
local graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.RendRecipeContextVK, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local function pipeline(key: string): graph.Resource
        return graph.graphics_pipeline(key, { pipeline = key, color_formats = { context.swap_chain_format } })
    end
    local draw = graph.draw_mesh({
        mesh = graph.mesh("material_variants.cube"),
        pipeline = {
            default = pipeline("draw_cube_unshaded_pipeline"),
            tinted = pipeline("draw_cube_unshaded_tinted_pipeline"),
        },
        parts = { "opaque" },
        clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } } },
    }, {
        name = "draw_material_variants",
        bindings = { camera_info = graph.import("camera_info") },
    }):color_target(0, graph.import("swap_chain"))
    return graph.build({ tasks = { draw } })
end

return exports
```

- **Handles, not strings.** Declarations take a descriptor table and an optional debug `name` (`graph.buffer{}`,
  `graph.storage_buffer{}`, `graph.image{}`, `graph.storage_image{}`, `image:view(descriptor?)`, `graph.image_view(image,
  descriptor?)`, `graph.clamp_to_edge_sampler{}`); imports are `graph.import(id)` and `import:member(name)`. The build
  generates ids (`<kind>-<8 hex>`, stable across rebuilds) and emits only what tasks reach; names become Vulkan debug
  names, so name every resource. C++ reaches a declared resource only through
  `graph.build{ exports = { <name> = resource } }` and `RenderGraphVK::acquire_resource(<name>)`.
- **Typed constructors.** `graph.compute()`, `graph.draw()`, `graph.draw_mesh()`, ... are generated from the task classes
  and type-check their params. Each takes `(params, { name?, bindings?, trigger?, toggle? })` and returns a task handle;
  `name` is the debug name profiles and diagnostics show, and the build generates task ids (`<type>-<8 hex>`).
- **Explicit bindings.** `bindings` (and `task:bind()`, `:color_target()`, `:depth_target()`) map every name the task's
  shaders and render targets use, imports included; recipe task maps are strict, so an unbound name fails.
- **Explicit edges.** Order tasks with `graph.chain(resource, { a, b, c })`, `a:connect(b, resource)`,
  `a:connect_all({ b, c }, resource)`, or `b:inputs_from(a, { local_name = resource })`. Bindings never add edges.
- **C++ pieces.** Tasks that need application objects (ImGui, acceleration-structure builds) are supplied with
  `RendGraphRecipeImportsVK::add_task()` by the bindings and referenced with `graph.import_task(id)`, whose `:bind()`
  aliases the identifiers the C++ task uses. Everything else the application owns (buffers, acceleration structures,
  entities whose members shaders name) is an import the recipe declares; nothing is added to a built descriptor in C++.
- **Pure C++ descriptors** exist only for focused compiler tests (`RenderGraphVK::build_from_descriptor()`).

Read [references/recipes-and-scripts.md](references/recipes-and-scripts.md) before writing or changing a recipe, a feature
module, a context struct, a trigger or toggle, an imported task, or a new task type.

## Register named resources on the graph

Meshes, pipelines, and images that tasks name by key are registered on the graph by the recipe, never kept alive by the
application or looked up globally. Their handles carry the registration, which the build emits when a task reaches the
handle. Descriptions that depend on application choices (such as a mesh's buffer usages) come through the context:

```lua
local static_mesh = graph.mesh("static_mesh", context.static_mesh) -- a wbe.RDParamsMeshVK
local blur = graph.compute_pipeline("blur", { shader = "blur_shader" })
local scan = graph.shader_compute_pipeline("prefix_scan_shader") -- compiled from the shader of the same key
local texture = graph.registered_image("noise", { texture = "noise_texture" }) -- bound like an import
local named_elsewhere = graph.mesh("dynamic_mesh") -- no description: registered by another handle
```

Registrations persist across rebuilds and equal re-registrations compile nothing; different descriptions of one key fail.
The description's type selects its registrator. A custom description type gets an `IResourceRegistrator` (usually
`RRTyped<TParams>`), installed with `add_resource_registrator(registrator)`, and is used from Teal with
`graph.registered_resource(key, "<TParams name>", description, resource_type)`. `GlobalRenderCache` only dedupes compiled
objects between graphs.

## Core rules

- **Edges are the only ordering.** Each edge carries one resource, and both tasks must access it; stages and accesses are
  derived from the `RendResUsageVK` usages both report. Every read-write, write-read, and write-write conflict on a resource
  must be ordered by a chain of edges on that same resource, or construction fails. Read-read pairs do not conflict.
- **Tasks report their own accesses** through `append_accesses()`: reflected shader resources (`WBE_DESCRIPTOR` access),
  render targets, and fixed-function buffers. Aliases only map task-local ids to graph resources; graphics tasks render to
  `render_target_colors[<index>]` and `render_target_depth`. Unused aliases are rejected, and recipe tasks' maps resolve
  nothing to itself.
- **Resources.** Graph-owned resources are buffers, images, image views, and samplers. Everything owned elsewhere is an import,
  supplied through `RendGraphRecipeImportsVK::add()` (or `add_resource(id, *bindable)` in C++); composites register
  `<id>.<member>`, which recipes name with `graph.import(id):member(member)`. Buffers that need initial data are
  application-owned imports.
  Keep the description structs' `operator==` complete, because diffing relies on it.
- **Meshes** are drawn with `graph.draw_mesh()` (`RTGraphicsDrawMeshVK`, or `add_to_graph()` in C++), whose `mesh` is a
  `graph.mesh()` handle it imports the mesh under. Do not hand-assemble mesh draws.
- **Per-material pipelines.** `pipeline` maps material pipeline variants to pipelines:
  `pipeline = { default = ..., foliage = ... }`; a material without `pipeline_variant` is `default`. Draws batch per variant;
  an unmapped variant throws, and the pipelines must share render targets and one material texture contract.
- **Queues.** Compute tasks default to `GRAPHICS`; set `queue_family = "compute_1"` or `"compute_2"` to run asynchronously.
  Same-queue edges become pipeline barriers; cross-queue edges use timeline semaphores. Every task on another queue must
  reach a graphics task.
- **Threading.** Render objects and backend helpers are rendering-thread-confined. Submit cross-thread work through
  `IRenderer::add_job()`; do not add locks.
- **Descriptors.** The graph binds the renderer-owned descriptor heaps before recording each task; pipeline-run tasks push
  their reflected descriptor indices. Do not reintroduce descriptor sets or pipeline layouts.

Read [references/system-and-usage.md](references/system-and-usage.md) before changing edge validation, synchronization,
queue behavior, descriptor binding, rebuild diffing, subgraph flattening, registration, or presentation, or when choosing
among the built-in task types.

## Validate

```sh
python validation.py -t debug -- --gtest_filter='WBERenderGraph*:WBERendGraph*:WBERenderTaskRegistryVK*:WBERT*:WBETealDeclarations*'
```

The wrapper builds (including the Teal type check, which fails the build on a type error), runs the focused tests, then the
full unit suite and shader tests. Visual or synchronization changes also need a rendering executable run; see the
`white-bird-testing` skill. Do not invoke CMake directly or run lint.
