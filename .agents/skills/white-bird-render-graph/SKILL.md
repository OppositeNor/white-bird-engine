---
name: white-bird-render-graph
description: Build, extend, and diagnose White Bird Engine's Vulkan render graphs and their Teal recipes. Apply when registering a graph and its rebuild callback, writing or changing a render-graph recipe or feature subgraph module, declaring graph resources, imports, and task edges, adding or registering an IRenderTaskVK type and its RTParams struct, using triggers, toggles, imported C++ tasks, or subgraphs, choosing queues, or debugging render-graph synchronization, rebuild diffing, or lifecycle behavior.
---

# White Bird Render Graph

A render graph is a DAG of `IRenderTaskVK` nodes connected by resource dependency edges. Applications never build a graph
directly: they register a **rebuild callback** that returns a complete `RendGraphDescVK`, and the graph calls it for the first
build and every rebuild. The descriptor usually comes from a Teal **recipe**; C++ adds only what recipes cannot express.

## Register a graph

```cpp
Ref<RenderGraphVK> graph = renderer->register_render_graph("my_graph",
    [this, renderer](const RendGraphContextVK& p_context, RenderGraphVK& p_graph) {
        RendGraphRecipeImportsVK imports;
        imports.add(REND_RES_SWAP_CHAIN, *renderer->acquire_swap_chain_image_view_container())
            .add("camera_info", *camera_info_ubo);
        const DataValue recipe = RendGraphScriptVK::run("render_graphs.my_graph",
            RendGraphScriptVK::make_context(*render_context, p_context));
        return RendGraphRecipeBuilderVK().build(recipe, imports, render_context, &p_graph);
    });
renderer->activate_render_graph("my_graph");
```

- The callback runs at registration, when the context changes (swap-chain extent, checked at `execute()`), and on
  `RenderGraphVK::rebuild()`. Keep pipeline compiles, application-owned buffers, and initial-data snapshots outside it; only
  assemble the descriptor inside, and make sure its captures outlive the graph.
- Re-import renderer-owned objects such as the swap-chain container inside the callback; do not capture them once.
- Rebuilds diff the new descriptor against the current graph. Matching graph-owned resources keep their content; changed ones
  are recreated in place or replaced. All tasks are recreated from their factories, so state that must survive a rebuild
  lives outside the task.

## Write the recipe

A recipe is a Teal `script` resource under `res/assets/scripts/teal/render_graphs/` whose `build(context)` creates a graph,
adds handles and tasks to it, and returns `graph:build()`:

```lua
local type wbe = require("wbe")
local render_graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(_context: wbe.RendRecipeContextVK): wbe.RendGraphRecipeVK
    local graph = render_graph.new()
    local swap_chain = graph:import_resource("swap_chain")
    local camera_info = graph:import_resource("camera_info")
    graph:draw_mesh("run_static_mesh", {
        mesh = graph:add_mesh("model_cube_static.mesh"),
        pipeline = { default = graph:add_graphics_pipeline("draw_cube_unshaded_pipeline") },
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
  `RendGraphRecipeImportsVK::add_task()` and referenced with `graph:import_task(id)`. The callback may also add to the
  descriptor the builder returns, for parts not yet expressible in Lua.
- **Pure C++ graph.** Only for small tests or when a recipe is not available; see `vulkan_draw_triangle_test.cpp`.

Read [references/recipes-and-scripts.md](references/recipes-and-scripts.md) before writing or changing a recipe, a feature
module, a context struct, a trigger or toggle, an imported task, or a new task type.

## Register named resources on the graph

Meshes, pipelines, and images that tasks name by key are registered on the graph, in the callback or the recipe, never
kept alive by the application or looked up globally:

```cpp
// Load once, outside the callback; the ResourceManager owns the resources.
RefRes<RUMesh3D> sponza = resource_manager->get_resource<RUMesh3D>("sponza.mesh");
RefRes<RUGraphicsPipeline> pbr = resource_manager->get_resource<RUGraphicsPipeline>("forward_cook_torrance_pbr");
// Inside the callback:
p_graph.register_mesh("static_mesh", sponza);
p_graph.register_graphics_pipeline("main", pbr, RDParamsGraphicsPipelineVK{
    .color_formats = {ImageFormatVK::B8G8R8A8_SRGB}, .depth_format = ImageFormatVK::D32_SFLOAT});
```

```lua
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
