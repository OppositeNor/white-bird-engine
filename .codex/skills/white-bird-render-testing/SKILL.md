---
name: white-bird-render-testing
description: Capture and inspect White Bird Engine rendering-test PNGs, automate deferred lighting/voxel checks and clean shutdown on Linux/X11, and integrate shared capture hooks in rendering executables.
---

# Rendering test screenshots

## Inspect current implementation

Read the header-only shared utility in `tests/executable/function/render/render_test.hh`, the relevant test's `main()` and `render_update()`, and the capture contract in `include/function/render/i_renderer.hh`. Check `git diff` first for updated probes or user changes. Read `tests/executable/function/render/generate.json` only when adding, removing, or renaming a rendering executable. Do not assume old binaries or screenshots reflect current source.

## Shared test hooks

Every executable test that renders must:

- Include `render_test.hh` and call `WBE::on_exec_test_start()` at the beginning of `main()`, before application/renderer initialization and thread startup. This configures capture and optionally waits for process injection.
- Call `WBE::on_exec_test_render(renderer, current_frame)` from its render-thread `render_update()`, before any early return. Pass the renderer by reference and its monotonic frame counter, not the rotating frame slot or acquired swapchain image index.

The shared utility is header-only; there is no `render_test.cpp` to link. When adding a new rendering executable, add only that executable's source pattern to `tests/executable/function/render/generate.json`. Do not edit `executable_test.gen.cmake`.

The shared test utility owns environment parsing and one-shot trigger state. Keep those policies out of the engine. Use `IRenderer::request_frame_capture()` for capture, never a hard-coded engine screenshot path or environment hook. Renderer access is render-thread confined; requests originating elsewhere must use `IRenderer::add_job()`.

Do not add these hooks to non-rendering unit tests simply because they use renderer mocks.

## Capture and inspect

For iterative visual checks, build with `python build.py -t debug` (or the requested target) and run the application. Run only relevant unit tests during iteration; reserve `python validation.py` for final validation unless the user requests otherwise. Inspect the end of failure logs first. Shader/resource compilation is part of the Python build pipeline. Do not edit generated shaders to establish a permanent fix, bypass SPIR-V validation, or invoke CMake directly.

From the repository root, choose a fresh absolute PNG path whose parent exists:

```sh
WBE_WAIT_PROCESS_INJECT=0 WBE_CAPTURE_PNG=/tmp/wbe-triangle-check.png WBE_CAPTURE_FRAME=5 ./build/debug/bin/wbe_render_test_vulkan_draw_triangle_test
```

- `WBE_CAPTURE_PNG`: unset or empty disables capture; otherwise requires an absolute path. Choose a new path for each experiment to avoid mistaking stale output for success.
- `WBE_CAPTURE_FRAME`: nonnegative integer, default `5`; requests once when the renderer counter reaches or exceeds it. The actual capture occurs on the next tick that renders; zero means the first eligible frame, not a guaranteed zero-indexed image.
- `WBE_WAIT_PROCESS_INJECT=1`: prints the PID and waits for Enter before renderer initialization. Use when attaching Nsight or another injector. Leave unset or set to `0` for unattended captures; preserve stdin access when waiting interactively.

The test continues after capture; it does not automatically exit. Observe the capture log and output file, then close the test window or stop only the process you launched. A timeout or manual stop is not a clean test exit. Respect GUI/GPU execution approvals.

Open the actual PNG with the local image-viewing tool. Report the path, dimensions, selected frame, and visible result: expected scene content, geometry stability, textures, lighting, and any black frame or exploded vertices. A nonempty PNG alone is not visual validation. Compare equivalent camera/frame settings where possible; do not use image generation to manufacture test evidence.

## Interpret results

Vulkan capture requires swapchain transfer-source support and RGBA8/BGRA8 output; unsupported configurations and write failures throw. Inspect console/validation errors as well as the image. Do not silently substitute a desktop screenshot when testing renderer readback.

Readback stalls the GPU and can change timing. Verify crashes and synchronization fixes in separate runs with capture disabled, long enough to exceed the observed failure window. Report visual correctness separately from stability, including duration, exit reason, and outstanding validation errors; a good screenshot does not establish that a segfault is fixed.

## Automated deferred checks (Linux/X11)

Use [scripts/check_deferred.py](scripts/check_deferred.py) for the deferred executable's existing `R` lighting switch, `V` voxel overlay, and `F1` renderer capture controls. Re-read those handlers if they change. This helper does not build, run unit tests, or judge visual correctness. It requires `DISPLAY`, `xdotool`, `taskset`, and `libX11`; request GUI execution permission when the environment requires it.

From the repository root:

```sh
python .codex/skills/white-bird-render-testing/scripts/check_deferred.py --octree 2
python .codex/skills/white-bird-render-testing/scripts/check_deferred.py --octree 1
python .codex/skills/white-bird-render-testing/scripts/check_deferred.py --octree 2 --mode stability --duration 25
```

Run configurations sequentially. Each invocation creates a fresh directory under `build/render_checks/` containing `application.log`, `result.json`, and (in capture mode) five PNGs: mipmap lighting, two summed-area-table frames, and two voxel-overlay frames. `result.json` records actual captured frame numbers, paths, CPU affinity, elapsed time, and exit status. Open the PNGs to assess rotation, instance visibility, occlusion and occupancy; the helper's success does not establish visual correctness.

The helper limits the owned process to four available CPUs by default (`--cpus` overrides), removes inherited capture settings for stability runs, and targets only the window matching its child PID. It sends `WM_DELETE_WINDOW` through X11 for normal SDL shutdown. Do not substitute `xdotool windowclose`: it can destroy the drawable and cause `BadWindow` errors. Timeouts or forced cleanup are recorded as failures and never called clean exits.

The demo always renders two animated instances of the cube mesh. Use `--target`, `--repo`, and `--output-dir` for another build or artifact location. Read `--help` for timing controls.

On device loss or shader overflow, inspect that run's log before continuing the matrix. Do not silently increase octree capacity or alter shaders merely to obtain a capture. Report failed configurations separately from successful captures and capture-free stability runs.
