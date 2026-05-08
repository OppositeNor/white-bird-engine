# White Bird Engine — Conventions

Compact reference for AI agents. High info density, low fluff.

## Layered Architecture

Layers, from upper to lower. Upper layers may depend on lower layers; the reverse is **forbidden**.

| # | Layer      | Role                                                                |
|---|------------|---------------------------------------------------------------------|
| 1 | `global`   | Engine-wide primitives, allocators, STL-allocator wrappers.         |
| 2 | `function` | High-level engine subsystems (e.g. ECS, gameplay glue).             |
| 3 | `resource` | Resource units, asset loading, resource registry.                   |
| 4 | `core`     | Memory model (`Ref`, `Unique`, ...), jobs, math/util building blocks. |
| 5 | `platform` | OS / windowing / file system / renderer backends.                   |

Rules:
- Templates are **forbidden in the exposed APIs** of `function` and any layer above it (i.e. `function` and `global`) — public headers, member functions, free functions, and types of these layers must not be templated. Internal helper functions / implementation details (e.g. file-local `static` helpers in a `.cpp`, anonymous-namespace utilities) **may** use templates. Templates are unrestricted in `resource`, `core`, and `platform`.
- Do not catch exceptions outside tests. All engine errors are thrown as `std::runtime_error` and must crash the program.

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

Lint / format check: `python check_lint.py` (runs clang-tidy + clang-format `--style=file`).
License headers: `python check_license_headers.py`.

## Tests

Located under `tests/` and mirror the `src/` layer structure:

| Path                | Purpose                                                  |
|---------------------|----------------------------------------------------------|
| `tests/unit/`       | GoogleTest unit tests; one `*_test.hh` per subject file. |
| `tests/integration/`| Cross-subsystem tests.                                   |
| `tests/executable/` | Standalone test executables (renderer demos, etc.).     |
| `tests/benchmark/`  | google-benchmark perf tests.                             |

Run:

```sh
./build/debug/bin/wbe_unit_test
./build/debug/bin/wbe_unit_test --gtest_filter='WBESomeTest*'
```

Per-directory `CMakeLists.txt` use `file(GLOB *.cpp)`; just drop new files in.

## File / Directory Layout

- `include/<layer>/...` — public headers (mirrors `src/`).
- `src/<layer>/...` — implementation (`.cpp`).
- `src/generated/`, `include/generated/` — output of the reflection metaparser; do not hand-edit.
- `templates/` — Jinja templates consumed by `build_script/reflection/`.
- `res/`, `test_env_res/` — runtime assets.
- `dependencies/` — vendored third-party (do not modify).
- `docs/` — design notes.
- `todos/` — per-layer todo lists.

Header / source naming: `snake_case.hh` / `snake_case.cpp`. Header guards: `WBE_FILE_<UPPER_SNAKE>_HH`. One primary class per file; file name matches the class in `snake_case`.

## Memory Model

**Do not use `std::shared_ptr`, `std::weak_ptr`, `std::unique_ptr`, `std::make_shared`, `std::make_unique`, `std::allocator`** in engine code. Allowed only as short-lived locals inside a function body when interfacing with third-party APIs that require them.

Engine equivalents (in `core/memory/`):

| STL                  | Engine                                | Notes                                                       |
|----------------------|---------------------------------------|-------------------------------------------------------------|
| `std::shared_ptr<T>` | `Ref<T, AllocType>`                   | Strong owning ref-counted handle.                           |
| `std::weak_ptr<T>`   | `RefWeak<T, AllocType>`               | Non-owning observer of a `Ref`.                             |
| `std::unique_ptr<T>` | `Unique<T>`                           | Sole-owner handle.                                          |
| (raw `T*` view of a `Unique`) | `RefRaw<T, AllocType>`       | **Use-only** view; must NOT create or destroy the resource. |

Construct via factories, passing the allocator:

```cpp
Ref<Foo> r = make_ref<Foo>(allocator, args...);
Unique<Foo> u = make_unique<Foo>(allocator, args...);
```

Containers: use `Vector<T>` etc. from `global/stl_allocator.hh` rather than raw `std::vector`. Custom allocator types follow standard `allocator_traits` member names: `rebind::other`, `propagate_on_container_copy_assignment`, `propagate_on_container_move_assignment`, `propagate_on_container_swap`.

## Naming

- `snake_case` for files, variables, functions, members. `PascalCase` for types. `UPPER_SNAKE` for macros and constants of macro-like nature.
- Function parameters: prefix `p_` (e.g. `p_allocator`, `p_buffer_size`). Local variables: no prefix. Member fields: no prefix.
- **No abbreviations** unless they are universally well-known. Examples:
  - Forbidden: `ci` (use `create_info`), `mgr` (use `manager`), `tex` (use `texture`), `cfg` (use `config` only if widely understood, else `configuration`).
  - Allowed well-known: `MPSC` (multiple producers, single consumer), `SPSC` (single producer, single consumer), `i`/`j`/`k` as for-loop indices, `id`, `uuid`, `gpu`, `cpu`, `os`, `vk` (Vulkan).
- If a name is too long, each word may be shortened to its first **≥4** letters. Examples: `initiate` → `init`, `information` → `info`, `allocator` → `alloc`; use shortening only when it remains unambiguous (`init`, `info`, `config`, `descr`).
- Preserve domain spellings already used in the codebase (e.g. test labels) even if unusual.

## Style

- C++20. `.clang-format` is authoritative; key rules:
  - `PointerAlignment: Left`, `ReferenceAlignment: Left` (`T* p`, `T& r`).
  - `ColumnLimit: 125`.
  - No short single-line forms: functions, lambdas, blocks, `if`, loops, `case`, `enum` all break to next line.
  - `AlwaysBreakTemplateDeclarations: Yes`.
  - `BreakStringLiterals: false` — long error-message string literals stay on one line.
- Use `WBE_R6_NDC_DELETE_COPY_MOVE_OVERRIDE(ClassName)` etc. macros from `utils/defs.hh` for rule-of-N boilerplate.
- Use `WBE_NO_FALSE_SHARING` to pad hot atomics that risk contention.
- Prefer the engine's file system wrappers `Directory` / `Path` (in `platform/file_system/`) over raw `std::filesystem`. They are header-only thin wrappers around `std::filesystem` — use them in engine code; drop down to `std::filesystem` only inside their implementation or when an API genuinely requires it. Note: `Directory::get_dir_names()` returns by value, capture with `auto`.
- For unbounded counting semaphores, use `std::counting_semaphore<>` (max = `PTRDIFF_MAX`); do not pick a small `LeastMaxValue` unless you can prove the bound.

## Reflection / Codegen

- All C++ header code are scanned by `build_script/reflection/metaparser.py`.
- Generated artifacts live in `*.gen.*` files; regenerated automatically each build. Do not edit by hand.
- Templates for codegen live in `templates/*.jinja`.
