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

A recipe is a Teal `script` resource under `res/assets/scripts/teal/render_graphs/` whose `build(context, parameters)` creates a graph,
adds handles and tasks to it, and returns `graph:build()`:

```lua
local type wbe = require("wbe")
local render_graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.RendRecipeContextVK, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local graph = render_graph.new()
    local swap_chain = graph:import_resource("swap_chain")
    local camera_info = graph:import_resource("camera_info")
    graph:draw_mesh("run_static_mesh", {
        mesh = graph:register_mesh("model_cube_static.mesh", { mesh = "model_cube_static.mesh" }),
        pipeline = { default = render_graph.graphics_pipeline("draw_cube_unshaded_pipeline", {
            pipeline = "draw_cube_unshaded_pipeline", color_formats = { context.swap_chain_format } }) },
        parts = { "opaque" },
    }, { uses = { camera_info } }):color_target(0, swap_chain)
    return graph:build()
end

return exports
```

- **Handles, not strings.** Resource methods (`graph:add_image()`, `image:view()`, `graph:import_resource()`, ...) and
  registered-resource declarations (`graph:register_mesh()`, `graph:register_graphics_pipeline()`, ...,
  `graph:add_mesh()`, `graph:add_compute_pipeline()`, `graph:add_graphics_pipeline()`) return
  `render_graph.Resource` handles. Pass them to task params, `:bind()`, `:color_target()`, `:depth_target()`, edges, and subgraph
  ports. `graph:build()` emits only the resources reachable from tasks, so there is no resource list to maintain.
- **Typed constructors.** `graph:compute()`, `:draw()`, `:draw_mesh()`, ... are generated from the task classes and
  type-check their params. Each takes `(id, params, { bindings?, uses?, trigger?, toggle? })` and returns a task handle.
- **Explicit edges.** Order tasks with `graph:chain(resource, { a, b, c })`, `a:connect(b, resource)`,
  `a:connect_all({ b, c }, resource)`, or `b:inputs_from(a, { local_name = resource })`. `uses` and bindings never add edges.
- **C++ pieces.** Tasks that need application objects (ImGui, acceleration-structure builds) are supplied with
  `RendGraphRecipeImportsVK::add_task()` by the bindings and referenced with `graph:import_task(id)`. Everything else the
  application owns (buffers, acceleration structures, entities whose members shaders name) is an import the recipe
  declares; nothing is added to a built descriptor in C++.
- **Pure C++ descriptors** exist only for focused compiler tests (`RenderGraphVK::build_from_descriptor()`).

Read [references/recipes-and-scripts.md](references/recipes-and-scripts.md) before writing or changing a recipe, a feature
module, a context struct, a trigger or toggle, an imported task, or a new task type.

## Register named resources on the graph

Meshes, pipelines, and images that tasks name by key are registered on the graph by the recipe, never kept alive by the
application or looked up globally. Descriptions that depend on application choices (such as a mesh's buffer usages) come
through the context:

```lua
local static_mesh = graph:register_mesh("static_mesh", context.static_mesh) -- a wbe.RDParamsMeshVK
graph:register_compute_pipeline("blur", { shader = "blur_shader" })
local scan = render_graph.shader_compute_pipeline("prefix_scan_shader") -- registered when a task names it
local texture = graph:register_image("noise", { texture = "noise_texture" }) -- bound like an import
```

Registrations persist across rebuilds and equal re-registrations compile nothing. The description's type selects its
registrator. A custom description type gets an `IResourceRegistrator` (usually `RRTyped<TParams>`), installed with
`add_resource_registrator(registrator)`, and is used from Teal with
`graph:register_resource(key, "<TParams name>", description, resource_type)`. `GlobalRenderCache` only dedupes compiled
objects between graphs.

## Core rules

- **Edges are the only ordering.** Each edge carries one resource, and both tasks must access it; stages and accesses are
  derived from the `RendResUsageVK` usages both report. Every read-write, write-read, and write-write conflict on a resource
  must be ordered by a chain of edges on that same resource, or construction fails. Read-read pairs do not conflict.
- **Tasks report their own accesses** through `append_accesses()`: reflected shader resources (`WBE_DESCRIPTOR` access),
  render targets, and fixed-function buffers. Aliases only map task-local ids to graph resources; graphics tasks render to
  `render_target_colors[<index>]` and `render_target_depth`. Unused aliases are rejected.
- **Resources.** Graph-owned resources are buffers, images, image views, and samplers. Everything owned elsewhere is an import,
  supplied through `RendGraphRecipeImportsVK::add()` (or `add_resource(id, *bindable)` in C++); composites register
  `<id>.<member>`, which recipes name with `graph:reference()`. Buffers that need initial data are application-owned imports.
  Keep the description structs' `operator==` complete, because diffing relies on it.
- **Meshes** are drawn with `graph:draw_mesh()` (`RTGraphicsDrawMeshVK`, or `add_to_graph()` in C++), which imports the mesh
  under its registration key on the graph. Do not hand-assemble mesh draws.
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
