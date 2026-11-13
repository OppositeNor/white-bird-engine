# White Bird Engine Agent Guide

## Project Overview

This is a C++23 engine project.

Follow repository conventions and existing patterns.

---

## General Rules

- Make the smallest change that solves the problem.
- Do not modify unrelated code.
- Do not modify generated files.
- Do not modify dependencies.
- Preserve existing APIs unless explicitly required.
- Use only one named namespace: `WhiteBirdEngine`. Do not introduce other named namespaces in engine or test-support code unless the user explicitly asks for it. Use anonymous namespaces for internal linkage in `.cpp` files instead of `static` free functions, in accordance with clang-tidy.

---

## Agent Workflow

Before implementing a feature or utility, check whether it already exists, whether an equivalent is embedded in another
implementation, or whether existing implementation-specific logic should be abstracted and reused.

For non-trivial implementation tasks:

1. Use repo-explorer when repository context is unclear.
2. Use investigator first for debugging, root cause analysis, and other investigation work.
3. Start implementation only after the investigation findings are clear.

---

## Change Discipline

- Make the smallest change that solves the problem.
- Do not refactor unrelated code.
- Do not rename existing APIs unless explicitly requested.
- Do not modify generated files.
- Do not modify dependencies.
- Before editing multiple files, verify that each file is required.

## Default-Safe Convention

- Procedures must default to crashing on failure. The ordinary API must not silently return a sentinel, invalid object, or
  partial result when it cannot fulfill its contract.
- Provide an explicitly named `try_` alternative only when failure is an expected, recoverable outcome. For example,
  `allocate()` crashes if allocation fails, while `try_allocate()` may return `MEM_NULL` to report an allowed allocation
  failure.
- Private helpers and other implementation details may omit runtime checks for assumptions guaranteed by their callers, such as
  pointer parameters being non-null, when avoiding the check is useful for performance. Express each such precondition with
  `WBE_DEBUG_ASSERT` at the point of use.
- `WBE_DEBUG_ASSERT` is a valid guard under this convention for implementation assumptions. Do not substitute a silent release
  fallback unless failure is explicitly part of the API contract.

## Rendering Threading

- Render objects and renderer-backend helpers are confined to the rendering thread. Their methods are not required to provide
  internal synchronization.
- Code running on any other thread must not access or operate on render objects directly. Submit that work through the
  renderer's job buffer with `IRenderer::add_job()` so it executes on the rendering thread.
- Do not add locks to render objects or renderer-backend helpers solely to support cross-thread access; preserve the
  render-thread confinement contract instead.

## Vulkan Render Graph

- Render graphs are built only from resource dependency edges declared with `RendTaskDescVK::connect(next_task_id, resource_id)`;
  the graph must be a DAG. Edge stages and accesses are derived from the `RendResUsageVK` usages both tasks report for the
  resource, and both tasks must access it. Declared reads and writes do not create edges. Every read-write,
  write-read, and write-write conflict on a resource must be ordered by a chain of edges on that same resource; unordered conflicts
  make graph construction fail. Same-queue edges become pipeline barriers and cross-queue edges use timeline semaphores.
- The graph compiler allocates graph-owned buffers, images, image views, and samplers declared with `RendGraphDescVK::add_buffer()`,
  `add_image()`, `add_image_view()`, and `add_sampler()`. Use `add_resource(id, *bindable)` only to import resources owned outside
  the graph; every import is an `IBindableRenderResourceVK`, and composites register their members under `<id>.<member>`.
- Tasks report every access themselves through `IRenderTaskVK::append_accesses()` (reflected shader resources, render targets, and
  fixed-function buffers); there is no descriptor-level access declaration. Graphics tasks render into `render_target_colors[<index>]`
  and `render_target_depth`. The only way to map task-local identifiers to graph resources is `RendTaskDescVK::alias()`, backed by
  the task's `RenderTaskResourceMap`; tasks do not take resource maps. Unmapped identifiers resolve to themselves only in
  C++-built descriptors and imported tasks; recipe tasks' maps are strict.
- Applications compose complete graphs only in Teal. A graph definition is a manifest `render_graph` resource
  (`RURenderGraph`, `RUSchemaRenderGraph` in `function/render/render_resource_unit.hh`) naming its `recipe` script and
  static `parameters`; definitions live in `res/assets/render_graphs/render_graphs.toml`. Acquire an instance with
  `IRenderer::acquire_render_graph(definition_key, bindings)` and select what renders with
  `activate_render_graph(graph)` (null clears it); both run on the rendering thread. There is no key-based registration
  and no application rebuild callback.
- Bindings (`IRenderGraphBindings`, Vulkan `IRenderGraphBindingsVK::prepare()`, or `RenderGraphBindingsFunctionVK`) carry
  an immutable instance key and prepare, for every build, the serialized application context and the
  `RendGraphRecipeImportsVK` (runtime resources and native task factories). They never see the descriptor or the graph,
  and must not retain their graph; the graph retains them. The graph supplies the renderer facts of the context and the
  `REND_RES_SWAP_CHAIN` import (when the recipe declares it and the bindings do not), acquired again on every build.
- Instances are cached weakly in `GlobalRenderCache` under `RenderGraphInstanceCache::make_cache_key(definition,
  instance)`: the same definition, instance key, and bindings object reuse the live graph, another bindings object for a
  live identity throws, and different instance keys are independent graphs. Callers (applications, `Scene`, and the
  renderer's activation) hold graphs strongly and must release them before the renderer shuts down. A `Scene` acquires one
  instance per distinct world definition and instance key, with the bindings supplied by `add_render_graph_bindings()`,
  and queues acquisitions, activations, and releases through `add_job()`; its teardown leaves the activation in place.
- A compiled `RenderGraphVK` keeps its recipe source (`RendGraphRecipeSourceVK`) and re-runs it for the first build, on a
  context change, and on `rebuild()`; an unchanged frame runs no script and prepares no bindings. Use buffer updates and
  toggle/select/trigger events for nonstructural changes. `RenderGraphVK::build_from_descriptor()` builds a fixed C++
  descriptor and exists only for focused compiler tests.
- Named resources that tasks use (meshes, pipelines, images) are registered on their graph (`IRenderGraph`, in
  `function/render/i_render_graph.hh`) in two steps: load the source resources into the `ResourceManager`, then
  `register_resource(key, description)`. The description's type (`IResourceDescription::get_type_uuid()`) selects the
  `IResourceRegistrator` that compiles it, as a resource schema's type selects its loader; there is no separate kind.
  `RenderGraphVK` installs `RRMeshVK`, `RRImageVK`, `RRGraphicsPipelineVK`, and `RRComputePipelineVK`, whose descriptions
  are the `WBE_REFLECT_SCRIPT` structs `RDParams<Name>VK` held in an `RDTyped<TParams>`. Recipes register them; the C++
  `RenderGraphVK::register_mesh(key, mesh_handle, options)` and the like name the handle's resource key, so they equal
  the key-based and Teal registrations (anonymous resources cannot be registered), and `add_resource_registrator()` installs one registrator per further
  description type. The graph holds registered objects strongly, keys are graph-local, registrations survive rebuilds, and
  re-registering a key with an equal description compiles nothing, so recipes register on every rebuild. Do
  not keep `Ref`s to registered objects just to keep them alive; acquire them with `acquire_registered<T>(key)`. Task
  types resolve registered resources through `RendGraphDescVK::get_render_graph()` in `prepare_graph()`, or by declaring
  `create(const RenderContextVK*, IRenderGraph*, const TParams&, const RendGraphContextVK&)`.
- `GlobalRenderCache` (`function/render/global_render_cache.hh`, built on `core/cache/weak_cache.hh`'s `WeakCache`) is the
  renderer's weak cache of compiled objects, keyed by source resource and object type, which compilers use to share
  compiled objects between graphs. Applications do not look up named resources there.
- Rebuilds diff the new descriptor against the current graph (`RendGraphResourceDiffVK`): a graph-owned resource is kept,
  with its content, only when its id, kind, per-frame layout, and evaluated description match and, for a view, its image
  is kept; changed configurations are recreated in place in their containers, while kind or per-frame changes get a new
  container. Imports are matched by object identity. Keep the description structs' `operator==` complete when adding
  fields, since the diff relies on them.
- The render graph binds the renderer-owned descriptor heaps before recording each task. Pipeline-run tasks push their reflected
  descriptor-index data; do not reintroduce descriptor-set or pipeline-layout objects.

### Render Graph Recipes

- A recipe is a Teal `script` resource whose module exports `build(context, parameters)`, where `parameters` is its
  definition's static map (type it with a `WBE_REFLECT_SCRIPT` struct when the recipe reads it). Recipes are pure
  descriptions: bind the library as `local graph = require("render_graph_utils.render_graph_utils")`, declare resources
  and tasks as free-standing handles with its module functions, and return `graph.build({ tasks = { ... }, exports = { ... } })`,
  which emits the plain `RendGraphRecipeVK` table (`imports`, `registrations`, `resources`, `tasks`, `exports`; see
  `rend_graph_recipe_params_vk.hh`). There is no graph object. `RendGraphScriptVK` runs the recipe and
  `RendGraphRecipeBuilderVK` turns it into a `RendGraphDescVK`; recipes only build descriptors and never touch render objects.
- `context` holds `RendGraphScriptVK::make_context()` facts (`RendRecipeContextVK`: `frames_in_flight`,
  `swap_chain_format`, `swap_chain_extent`). An application that adds entries declares a `WBE_REFLECT_SCRIPT` struct
  deriving from `RendRecipeContextVK`, fills its base with the renderer facts its bindings receive, and returns
  `RendGraphScriptVK::serialize_context()` as the prepared context. Size swap-chain-dependent resources and dispatches in
  Lua from `swap_chain_extent`.
- A resource is either declared by the recipe or imported. Declarations take one descriptor table of the reflected
  parameters plus an optional debug `name`: `graph.buffer{}`, `graph.storage_buffer{}`, `graph.image{}`,
  `graph.storage_image{}`, `image:view(descriptor?)` (no argument: the image's default view, which takes its name),
  `graph.image_view(image, descriptor?)`, and `graph.clamp_to_edge_sampler{}`. The build generates their ids,
  `<kind>-<8 hex>` from the kind, the name, and the declaration's ordinal among those of that kind and name in the order the
  build reaches them, so rebuilds keep every id (and the resources' contents) and a named resource keeps its id when others
  are added. The name becomes the Vulkan debug name of the objects the graph creates (`RendGraphDescVK::set_debug_name()`)
  and appears in diagnostics; ids are meaningless to read.
- Every resource owned outside the graph is `graph.import(<C++ id>)` and supplied through `RendGraphRecipeImportsVK`; a
  declared-but-missing or supplied-but-undeclared import is an error, except that an unsupplied import resolves to a
  bindable object registered on the graph under its name (`graph.registered_image()`). Composite imports expose their
  members as `import:member(<member>)`. Recipes cannot set initial data, so buffers that need it are application-owned
  imports. C++ reaches a recipe-declared resource only through an export: `graph.build{ exports = { <name> = resource } }`
  and `RenderGraphVK::acquire_resource(<name>)`.
- Registered entries are handles that carry their registration: `graph.mesh(key, description?)`,
  `graph.compute_pipeline(key, description?)`, `graph.graphics_pipeline(key, description?)`,
  `graph.shader_compute_pipeline(shader)` (registered from the shader of the same key), `graph.registered_image(key,
  description)` (an import), and `graph.registered_resource(key, type, description, resource_type)` for custom description
  types. Without a description a handle names an entry registered elsewhere. A registration is emitted, as
  `registrations` (`{ key, type, description }`, where `type` is the reflected name of the description struct, resolved
  through `TypeUUIDManager`), when a built task's params or bindings reach its handle; equal registrations of a key merge and
  different ones fail. They are applied before that level's imports and tasks.
- Tasks are free-standing handles made by the generated typed constructors, `graph.<constructor>(params, { name?, bindings?,
  trigger?, toggle? })`, by `graph.task(type, params, options)` for a registered type without a constructor (params that
  name no resource), or by `graph.import_task(<C++ id>)`. `graph.build()` takes the tasks in order, generates their ids
  (`<type>-<8 hex>`, like resource ids; imported tasks keep theirs), and emits `name` as the task's debug name
  (`RendTaskDescVK::debug_name`), which profiles and diagnostics show. A task listed twice, or an edge to a task the build
  does not list, is an error.
- Shader names bind explicitly: a task's `bindings` (and `task:bind()`, `color_target()`, `depth_target()`) map every name
  its shaders reflect and its render targets use to a resource handle, imports included. Recipe tasks' resource maps are
  strict (`RenderTaskResourceMap::require_bindings()`), so an unbound name fails instead of matching a resource by id.
  Edges come only from explicit connections (`graph.chain()`, `task:connect()`, `connect_all()`, `inputs_from()`),
  recorded on the task handles and emitted as `connect` entries (`{ to = <task id>, resource = <resource id> }`). An
  optional `trigger` wraps the task with `RTTriggeredVK`, and an optional `toggle` (`{ event, enabled }`) wraps it with
  `RTToggleVK`, outside any trigger. Rebuilds re-run the recipe, so pass a toggle's current state through the context.
- A task that records one of several alternatives is a multiplexer (`RTMultiplexerVK`):
  `graph.multiplex{ name?, branches = { <typed tasks> }, select = { event, branch }, bindings?, trigger?, toggle? }`. It is
  one graph node reporting every branch's accesses; the branches share its bindings and one queue family, carry no edges,
  triggers, or toggles, and `RTMultiplexerVK::Command` events select the branch. Pass the current branch through the
  context, as for a toggle.
- Task `pipeline` params are registered pipeline handles.
- Draw meshes with `graph.draw_mesh` (`RTGraphicsDrawMeshVK`, or `RTGraphicsDrawMeshVK::add_to_graph()` in C++); do not
  assemble `RTRunGraphicsPipelineIndirectVK` mesh draws by hand. Its `mesh` is a `graph.mesh()` handle, which it imports
  the mesh under; a key nobody registered is registered from the mesh resource of that key with the default options.
  `pipeline` maps material pipeline variants to pipelines (`pipeline = { default = ..., foliage = ... }`); draws batch per
  variant, and the task's pipelines must share render targets and one material texture contract.
- Everything owned by the application that a recipe or shader names, such as an acceleration structure or composite
  entities whose members shaders read, is an explicit import supplied by the bindings; nothing is added to a built
  descriptor in C++.
- A C++ feature that contributes to a recipe describes itself with a `configure_recipe()` that serializes the
  `WBE_REFLECT_SCRIPT` parameters struct of its Lua feature module (including the `module` name) into the context and supplies its resources and C++ tasks to the
  `RendGraphRecipeImportsVK`; the recipe calls `require(params.module).subgraph(name, params, target)`, which returns the
  feature as a subgraph task. Work that runs once, such as a static
  build, is a `trigger` activated while it is still pending; the application disables the trigger event after the first
  recorded frame, so later rebuilds create it inactive. Task state that must follow runtime input across rebuilds is a
  `toggle` whose event the application emits and whose current state it passes through the context.
- A subgraph groups tasks, graph-owned resources, and internal edges under one id (`RendGraphDescVK::add_subgraph()`;
  in a recipe, `graph.subgraph{ name?, tasks = { ... }, ports = { <port> = <enclosing resource> }, trigger?, toggle? }`,
  whose tasks name the ports with `graph.port(<port>)`; it emits a task with a nested recipe in `subgraph` and its bound
  `ports`). It is flattened when it is added: its task and owned resource ids become `<id>.<name>`, its debug names
  `<subgraph name>.<name>`, and names inside it resolve to a port's bound resource, else its own resource, else the
  enclosing graph's. Tasks inside a subgraph reach the enclosing graph's declared resources only through ports; imports stay
  graph-wide, and the same resource imported again merges. Edges to and from the subgraph id resolve when the graph is
  built, from the tasks' accesses: an incoming edge on R reaches the members that access R with no edge on R from another
  member, and an outgoing one leaves the members that access R with no edge on R to another member, so order a subgraph's
  accesses to its ports with internal edges. Members are ordinary tasks that may be connected directly, and a subgraph
  task's `trigger` or `toggle` wraps every member. Only the outermost recipe exports.
- Use `full_memory_barrier` (`RTFullMemoryBarrierVK`) to order a frame's mutation of persistent state after the previous
  frame's consumers, and `run_graphics_pipeline_indirect` for non-mesh indexed indirect draws.
- Tasks that need application objects (for example the ImGui overlay) are built in C++ and supplied with
  `RendGraphRecipeImportsVK::add_task()`; the recipe references each with `graph.import_task(id)` (emitted as
  `{ id = <task id>, import = true }`) and connects it like any task. Imported tasks set no `type`, `params`, `trigger`, or
  `toggle`; their `bindings` alias the identifiers the C++ task uses to recipe resources, and what they leave unmapped
  resolves to itself. Supplied and imported tasks must match, like resource imports.
- Recipes and their feature modules are Teal. `templates/teal_types.d.tl.jinja` generates the Teal declarations of the
  `WBE_REFLECT_SCRIPT` classes and enums (the `reflect_script` attribute group; such a class is also a static serializable
  and such an enum a reflected enum) into `build/<target>/teal/wbe.d.tl`; the test executables' types appear only in targets
  that build the tests, the only ones reflection scans them in. The ACP type-checks scripts against them, so reflection
  runs before the ACP. Every script class is an
  interface, a derived one `is` its bases, and a script type naming a reflected type that is not `WBE_REFLECT_SCRIPT`
  fails generation. Declare data C++ hands a recipe as `WBE_REFLECT_SCRIPT` structs rather than Teal records, import the
  declarations as types only (`local type wbe = require("wbe")`), and annotate `build(context: <context type>):
  wbe.RendGraphRecipeVK`. Reflected enum fields are declared as their Teal enum of names and aliases, without integers:
  enums serialize as their canonical names; compare `graph.enum_name(value)` with the canonical upper-case name, which
  also accepts the lowercase aliases. The typed constructors check params against the generated
  `graph.RTParams<Name>VK` interfaces, and the resource descriptors against `graph.BufferDescriptor` and the like, which
  extend the reflected parameter structs. The generated repo-root `tlconfig.lua` points the Teal language server at the
  same paths for the last built target.
- The typed task constructors (`graph.compute()`, `graph.draw_mesh()`, ...) are generated into `render_graph_utils.render_graph_tasks`
  (`templates/teal_render_graph_tasks.tl.jinja`) from the task classes annotated `WBE_RENDER_TASK(<method>)` and installed
  on the module; each creates the task type of its class's first `WBE_SERIALIZATION_ALIASES` entry, and a bare
  `WBE_RENDER_TASK` only registers the type. Mark params fields that hold resource ids `WBE_RECIPE_RESOURCE` (graph
  resources) or `WBE_RECIPE_RESOURCE(registered, <RenderObjectType>)` (keys registered on the graph) so the constructor
  takes resource handles there, and only handles; the build lowers a graph handle to its id with an identity alias.
  Do not hand-write constructors or handle-bearing params records.

### Scripts

- Script sources live under `res/assets/scripts/teal/` and are `script` resources declared in
  `res/assets/scripts/teal/lua_scripts.toml`: recipes in `render_graphs/`, reusable recipe parts (modules whose
  `subgraph(id, params, target)` returns a subgraph task) in `render_graph_utils/features/`, graph helpers in
  `render_graph_utils/`, and general script helpers in `utils/`.
- A script's resource id is its module name: its path under `scripts/teal/` without its extension, with dots for slashes
  (`render_graph_utils/render_graph_utils.tl` is `render_graph_utils.render_graph_utils`). Keep the two in sync, so that
  `require` resolves the same module at runtime (through `SMRResourceManager`) and in the editor.
- Scripts may be written in Lua (`.lua`) or Teal (`.tl`). `ACPCompilerScript` copies Lua sources and type-checks and
  compiles Teal sources to `<name>.lua` with the vendored compiler in `dependencies/tl`, run in-process through `lupa`;
  a type error fails the build. Teal modules resolve `require` against `res/assets/scripts/teal/` at compile time, so a
  Teal module can require a Lua module only through a `.d.tl` declaration next to it.
- Modules that reflection generates (`render_graph_utils.render_graph` and `render_graph_utils.render_graph_tasks`) are
  written to `build/<target>/teal/`, which `require`
  also resolves against. Their `lua_scripts.toml` entries give `source_file` relative to that directory, where the ACP
  looks first, and set `path` under `assets/scripts/teal/`.
- `require` is the only import mechanism. It loads each module once per backend and shares its export table, and cyclic
  requires fail. Implement new import sources as an `IScriptModuleResolver`, whose `try_resolve()` must not throw.

## Layered Architecture

Layers, from upper to lower. Upper layers may depend on lower layers; the reverse is **forbidden**.

| # | Layer      | Role                                                                |
|---|------------|---------------------------------------------------------------------|
| 1 | `global`   | Engine-wide primitives         |
| 2 | `function` | High-level engine subsystems (e.g. ECS, gameplay glue).             |
| 3 | `resource` | Resource units, asset loading, resource registry.                   |
| 4 | `core`     | Allocators, STL-allocator wrappers, Memory model (`Ref`, `Unique`, ...), jobs, math/util building blocks. |
| 5 | `platform` | OS / windowing / file system / renderer backends.                   |

Rules:
- Templates are **forbidden in the exposed APIs** of `global` — its public headers, member functions, free functions, and types must not be templated. Internal helper functions / implementation details (e.g. file-local `static` helpers in a `.cpp`, anonymous-namespace utilities) **may** use templates. Templates are unrestricted in `function`, `resource`, `core`, and `platform`.
- Do not catch exceptions outside tests. All engine errors are thrown as `std::runtime_error` and must crash the program.

## Validation

Use the validation wrapper for normal build and unit-test validation. It prints only short `WBEValidator:` status lines, writes detailed logs under `build/validation_logs/`, and stops at the first failed step.

```sh
python validation.py                         # debug build + full unit tests
python validation.py -t release              # release build + full unit tests
python validation.py -t debug-gcc            # GCC debug build + full unit tests
python validation.py -t debug -- --gtest_filter='WBESomeTest*'
```

When gtest arguments are forwarded after `--`, the requested gtest run is executed first. If it passes, validation still runs the full unit test binary afterward.

Do not include lint in validation. The user performs lint checks manually. Do not make lint or formatting fixes unless the
user explicitly asks for them.

## Build

Always use the Python build script — never invoke `cmake` directly.

```sh
python build.py                 # default: deploy
python build.py -t debug        # debug build  (clang)
python build.py -t release      # release build (clang)
python build.py -t deploy       # deploy build  (clang)
python build.py -t debug-gcc    # debug build with GCC
python build.py -t release-gcc  # release with GCC
```

Build outputs land in `build/<target>/`. Test binaries: `build/<target>/bin/`, e.g. `build/debug/bin/wbe_unit_test`.

### Projects

- A game in another repository is a project: a directory with a `wbe_project.toml` (`[project]` with `name`,
  `include_dirs`, `source_dirs`, `resource_dir`, `generate_files`; see `build_script/project/project.py`) and a `CMakeLists.txt`.
  Build it with `python <engine>/build.py -t <target> --project <dir>`; output lands in `<dir>/build/<target>/`, and the
  engine's tests are not built.
- Reflection is closed-world: the project's headers are reflected together with the engine's, and the generated registries
  are compiled into the engine library. The project's `CMakeLists.txt` therefore adds sources defining what its reflected
  headers declare with `wbe_add_project_sources()`, and its executables with `wbe_add_executable()`. Reflected project
  types must be in the `WhiteBirdEngine` namespace, since generated code names them unqualified.
- Project resources are compiled into the same runtime `res` directory after the engine's. Project shaders may include
  engine shaders, and project scripts may `require` engine modules; project `res/config` files override the engine's.

## Build/Test Logs

Build and test outputs can be large. Always check the end of the previous output first using `tail`.

- Success: do not read the full log.
- Failure: inspect only the relevant sections needed to diagnose the issue.
- Do not dump or scan entire logs unless necessary.

Manual-only checks: `python check_lint.py` for lint/format and `python check_license_headers.py` for license headers. Do not run these automatically as part of validation.

## Tests

Tests mirror the src layer structure.

Run:

./build/debug/bin/wbe_unit_test

Use:
--gtest_filter='Pattern*'

Per-directory CMakeLists.txt use `file(GLOB *.cpp)`; new test files are picked up automatically.

## File / Directory Layout

- `include/<layer>/...` — public headers (mirrors `src/`).
- `src/<layer>/...` — implementation (`.cpp`).
- `templates/` — Jinja templates consumed by `build_script/reflection/`.
- `res/` — source assets and manifests; ACP writes deployment resources to `build/<target>/bin/res` and test resources to `test_env/res`.
- `dependencies/` — vendored third-party (do not modify).
- `todos/` — per-layer todo lists.

Generated files (under the build directory, `build/<target>/`, or `<project>/build/<target>/` for a project build):
- `generated/src/`
- `generated/include/generated/` (included as `"generated/<name>.gen.hh"`)
- `teal/`

Never edit generated files.

Header / source naming: `snake_case.hh` / `snake_case.cpp`. Header guards: `WBE_FILE_<UPPER_SNAKE>_HH`. One primary class per file; file name matches the class in `snake_case`.

## Resource Pipeline / ACP

- ACP source manifests are TOML (`.toml`), parsed with Python's `tomllib`. ACP output stays JSON: source `<path>.toml` is
  emitted as runtime `<path>.json`, and runtime code refers to the `.json` path.
- ACP root manifest discovery starts only from `res/res_chunks/**/*.toml`.
- Do not make ACP scan every TOML file under a resource tree.
- Manifest files outside `res_chunks` are processed only when reachable from a root chunk manifest through `include`.
- `include` paths are resource-root-relative `.toml` manifest paths. Included files must contain a `resources` array.
- `scope` defaults to `global`. `global` resources compile for both deployment and tests; `test_only` resources compile only with tests enabled. Scope may be set on a manifest to skip its entire include tree, or on an individual resource. ACP validates scopes, filters before compiling, and removes scope from runtime manifests.
- `wbe-utils-model-compiler` owns final mesh and material resource shaping. Keep `acp_compiler_model.py` limited to ACP path/schema
  adaptation; do not normalize compiled material textures in the ACP adapter.
- Mesh SDFs (`sdf_config` on `model`, `static_geometry`, and `mesh`) are emitted by the model compiler's `MeshSDFBuilder`
  for both mesh compilers; only the injected host `SDFCompiler` (`SDFCompilerMeshToSDF`, mesh-to-sdf) samples distances. Do
  not sample SDFs anywhere else.

### Material Texture Contract

- Materials do not have a `material_type`. Their graphics pipeline and its reflected shader metadata define the rendering
  contract. A material may set a free-string `pipeline_variant` (`default` when absent); draw tasks map variants to compiled
  pipelines, so materials never name pipelines or passes.
- Every material texture slot has a non-empty, shader-defined `texture_role`; do not introduce a closed engine-side texture-role
  enum or whitelist.
- Shader texture roles are declared on integer fields of the global `MaterialParams` struct with `wbe::texture` in `utils/utils.slang`.
  - `[wbe::texture("albedo")] int albedo_index;` selects the explicit `albedo` role.
  - `[wbe::texture] int detail_index;` uses `detail_index` as the role.
- `MaterialTextureContractMetadata.material_sampler` and `MaterialTextureContractMetadata.textures` are fixed metadata-only
  members; `MaterialParamsBuffer.materials` is the fixed runtime material-table member. Applications may configure only the
  `material_textures` and `material_params` base variable names and the `default_material` resource key.
- Static-mesh compilation resolves textures to global sampled-image descriptor-heap indices and stores those indices in the reflected
  material-parameter table. Keep engine propagation generic; do not branch on concrete roles such as `albedo`, `normal`, or
  `rma`.
- Vulkan shader resources are declared as aligned `uint2` fields in push data with `wbe::descriptor("TYPE", "resource_id", "ACCESS")`,
  where `TYPE` is `uniform_buffer`, `storage_buffer`, `sampled_image`, `storage_image`, `sampler`, or `acceleration_structure`, and
  the optional `ACCESS` is `read`, `write`, or `read_write`. Storage buffers and images default to `read_write`; annotate read-only
  ones with `read`. Runtime shaders access the renderer-owned global heaps through those indices; metadata-only resource
  declarations belong under `WBE_REFLECTION_METADATA`.
- `rma` is the canonical project abbreviation and texture role for the roughness, metallic, and ambient-occlusion channel pack.
  Use `rma` in project-owned identifiers instead of a spelled-out equivalent.

## Configuration Files

- Engine and game configuration files (`res/config/**/*.toml`) are TOML, loaded with `ParserTOML` from
  `core/parser/parser_toml.hh` (toml++). The engine has no YAML parser.

## Memory Model

**Do not use `std::shared_ptr`, `std::weak_ptr`, `std::unique_ptr`, `std::make_shared`, `std::make_unique`, `std::allocator`** in engine code. Allowed only as short-lived locals inside a function body when interfacing with third-party APIs that require them.

- Resource handles and allocator-aware objects must document whether they are copyable, movable, or neither.

Engine equivalents (in `core/memory/`):

| STL                  | Engine                                | Notes                                                       |
|----------------------|---------------------------------------|-------------------------------------------------------------|
| `std::shared_ptr<T>` | `Ref<T, AllocType>`                   | Strong owning ref-counted handle.                           |
| `std::weak_ptr<T>`   | `RefWeak<T, AllocType>`               | Non-owning observer of a `Ref`.                             |
| `std::unique_ptr<T>` | `Unique<T>`                           | Sole-owner handle.                                          |
| (raw `T*` view of a `Unique`) | `RefRaw<T, AllocType, AllowDestruct>`       | **Use-only** view; must NOT create or destroy the resource. |

There are other aliases: `RefRes` is an alias of `RefRaw`, with `AllowDestruct` specified as false.

Construct via factories, passing the allocator:

```cpp
Ref<Foo> r = make_ref<Foo>(allocator, args...);
Unique<Foo> u = make_unique<Foo>(allocator, args...);
```

Containers: use `Vector<T, <HeapAllocatorType>>` etc. from `core/allocator/stl_allocator.hh` rather than raw `std::vector`. Custom allocator types follow standard `allocator_traits` member names: `rebind::other`, `propagate_on_container_copy_assignment`, `propagate_on_container_move_assignment`, `propagate_on_container_swap`.

## Special Member Functions (Rule of 6)

Classes that manage resources, ownership, or non-trivial state must explicitly follow the Rule of 6:

- Default constructor
- Destructor
- Copy constructor
- Copy assignment operator
- Move constructor
- Move assignment operator

Rules:
- If a class owns a resource, define or delete copy/move operations explicitly.
- Do not rely on compiler-generated copy/move operations for resource-owning types unless the behavior is verified to be correct.
- Prefer move semantics for ownership transfer.
- Copy operations must preserve ownership invariants and perform deep copies when required.
- Move operations must leave the moved-from object in a valid destructible state.
- Use `= delete` for unsupported operations instead of leaving accidental behavior.
- Use `= default` only when the generated implementation matches the intended ownership semantics.

## Naming

- `snake_case` for files, variables, functions, members. `PascalCase` for types. `UPPER_SNAKE` for macros and constants of macro-like nature.
- C++ function parameters: prefix `p_` (e.g. `p_allocator`, `p_buffer_size`). Teal, Lua, and Python function parameters: no `p_` prefix. Local variables: no prefix. Member fields: no prefix.
- **No abbreviations** unless they are universally well-known. Examples:
  - Forbidden: `ci` (use `create_info`), `mgr` (use `manager`), `tex` (use `texture`), `cfg` (use `config` only if widely understood, else `configuration`).
  - Allowed well-known: `MPSC` (multiple producers, single consumer), `SPSC` (single producer, single consumer), `i`/`j`/`k` as for-loop indices, `id`, `uuid`, `gpu`, `cpu`, `os`, `vk` (Vulkan), `rma` (roughness, metallic, and ambient-occlusion texture channel pack).
- If a name is too long, each word may be shortened to its first **≥4** letters. Examples: `initiate` → `init`, `information` → `info`, `allocator` → `alloc`; use shortening only when it remains unambiguous (`init`, `info`, `config`, `descr`).
- Preserve domain spellings already used in the codebase (e.g. test labels) even if unusual.

## Style

- C++23. `.clang-format` is authoritative; key rules:
  - `PointerAlignment: Left`, `ReferenceAlignment: Left` (`T* p`, `T& r`).
  - `ColumnLimit: 125`.
  - No short single-line forms: functions, lambdas, blocks, `if`, loops, `case`, `enum` all break to next line.
  - `AlwaysBreakTemplateDeclarations: Yes`.
  - `BreakStringLiterals: false` — long error-message string literals stay on one line.
- Declare every overriding member function, destructors included, with both `virtual` and `override`
  (`virtual void record(...) override;`, `virtual ~Foo() override = default;`). Never write `override` without `virtual`.
- Use `WBE_NO_FALSE_SHARING` to pad hot atomics that risk contention.
- Use the engine's `Path` alias from `platform/file_system/path.hh` for filesystem paths in engine APIs. It is `std::filesystem::path`; use standard path operations directly.
- For unbounded counting semaphores, use `std::counting_semaphore<>` (max = `PTRDIFF_MAX`); do not pick a small `LeastMaxValue` unless you can prove the bound.

## Header / Source Separation

Non-trivial method implementations must be placed in `.cpp` files.

Allowed inline definitions in headers:
- Simple getters/setters.
- Trivial methods with a short, obvious implementation.
- Functions that require header definitions (e.g. templates).

Avoid placing complex logic, control flow, or dependency-heavy code in headers. Public headers should expose interfaces and declarations rather than implementation details.

## API Design

- **Prefer exposing classes and interfaces over free-function wrappers.** When a feature has internal state, a constructable context, or a small set of related operations, declare the class in the public header and expose its members. Do not hide it behind a file-local impl class wrapped by free functions in the .cpp. Internal helpers, anonymous-namespace utilities, and detail types stay in the `.cpp`.
- **Name derived implementations with the interface (or its abbreviation) as a prefix.** For example, `RenderObject` → `ROImage`, `ROGraphicsPipelineVK`; `Renderer` → `RendererVK`; `RenderTask` → `RTRunGraphicsPipelineVK`. Pick a short, consistent abbreviation per interface and use it for all derived types.

## Documentation

Use Doxygen for C++ and Slang.

Place Doxygen commands in multiline comment blocks, with every command on its own `*` line. Do not use compact command comments
such as `/** @brief ... */` or combine commands such as `@class` and `@brief` on one line.

```cpp
/**
 * @class Example
 * @brief Describes the example.
 */
```

Required:
- Public/protected classes, structs, fields, functions.

Not required:
- Private members.
- Overrides unless behavior differs.

Document contracts, ownership, threading, and errors; do not restate signatures.

## Reflection / Codegen

- All C++ header code are scanned by `build_script/reflection/metaparser.py`.
- A `WBE_META` attribute is a bare name (`WBE_REFLECT`) or a name with parameters (`WBE_RENDER_TASK(compute)`), which
  metadata keeps as trimmed text in `attribute_parameters`; there is no assignment syntax. Declare serialization aliases as
  `WBE_SERIALIZATION_ALIASES(name, ...)`, each a name of letters, digits, and underscores.
- Generated artifacts live in `*.gen.*` files; regenerated automatically each build. Do not edit by hand.
- Templates for codegen live in `templates/*.jinja`.
- Each `[[generate]]` entry of a `generate.toml` supports `${name}` replacement in `output_name`, `template`, `out_dir`, and nested string values under `data`. Per-entry `params` override built-ins; built-ins include `build_target`, `build_dir`, `binary_dir`, `root_dir`, `include_dir`, `source_dir`, `project_dir`,
  and `project_script_dir` (the last two name the engine's own root and script directory when building without a project).

## Python Scripts

- All function parameters and return values must have type annotations. Use `-> None` explicitly for procedures.
- Prefer concrete generics (`list[str]`, `dict[str, Any]`) over bare `list` / `dict`. Use `from typing import Any, Callable` etc. when needed.
- Annotate non-trivial local variables whose type cannot be obviously inferred (e.g. empty containers: `result: list[str] = []`).
- Use PEP 604 unions (`X | None`) rather than `Optional[X]`.
- Do not prefix Python types with `WBE`; modules namespace them. Derived implementations still start with their base
  type's name (`ACPCompiler` → `ACPCompilerShader`). Log tags such as `WBEBuilder:` and `WBEACP:` are output text, not types.
