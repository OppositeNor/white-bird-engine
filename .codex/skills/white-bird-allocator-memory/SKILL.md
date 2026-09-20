---
name: white-bird-allocator-memory
description: Use White Bird Engine's allocator and memory modules when choosing an allocator, constructing allocator-owned objects, using Ref/RefWeak/Unique/RefRaw handles, or creating allocator-aware containers. Apply to C++ engine code that allocates or transfers ownership; do not use for unrelated memory profiling or GPU resource allocation.
---

# White Bird Allocator and Memory

Use the engine's ownership types and pass the allocator explicitly. The allocator is never owned by a handle or container: keep it alive until every object, container, `Ref` control block, and `RefWeak` observer using it is gone. `MemID` comes from a particular allocator and must be returned to that same allocator. `MEM_NULL` is the null ID.

## Choose ownership before allocating

| Need | API | Lifetime |
| --- | --- | --- |
| One owner | `Unique<T, AllocType>` via `make_unique<T>(allocator, args...)` | Move-only; destructor or `reset()` destroys the object and frees its storage. |
| Shared ownership | `Ref<T, AllocType>` via `make_ref<T>(allocator, args...)` | Last strong handle destroys the object; the control block remains until weak handles are gone. |
| Observe a shared object | `RefWeak<T, AllocType>` from a valid `Ref` | Call `lock()` and check the returned `Ref` before access; it does not extend object lifetime. |
| Use an existing resource without owning it | `RefRaw<T, AllocType, false>`; `RefRes<T>` is the resource alias | Copyable view. Its destructor and `delete_ref()` do not destroy the object. |
| Explicit manual ownership | `new_ref<T>(allocator, args...)` with the matching `delete_ref(std::move(handle))` | The free factory defaults to `AllowDestruct=true`. Copies are aliases; destroying one leaves the others dangling. Prefer `Unique` or `Ref` when they fit. |

Include the matching header under `include/core/memory/`. Do not use `std::shared_ptr`, `std::weak_ptr`, or `std::unique_ptr` for engine ownership. A `RefRaw` constructed from a `Unique` is a view; the `Unique` must outlive it. `RefRaw<T>::new_ref()` uses the class template's `AllowDestruct` parameter, whose default is **false**; pairing that default with `RefRaw<T>::delete_ref()` does not destroy the object. Use the free `new_ref`/`delete_ref` pair for manual ownership.

## Choose the allocator

- Use `HeapAllocatorDefault` from `core/core_utils.hh` for ordinary engine allocation where an allocator instance is available. `global_allocator()` in `core/engine_core.hh` accesses the engine's allocator when the engine is initialized; do not let its users outlive shutdown.
- Use `HeapAllocatorRAM` for a simple standalone heap allocator, including focused unit tests. Choose a specialized pool or arena only when its capacity, alignment, and threading properties fit the caller.
- Use `StackAllocator` for bounded LIFO work. Construct with `create_stack_obj<T>()` and pop in reverse order with `pop_stack_obj<T>()`; `clear()` invalidates outstanding IDs.
- Use `Vector`, `HashMap`, `String`, and other aliases in `core/allocator/stl_allocator.hh` with an explicit allocator type and instance. Their `STLAllocator` requires `AllocatorTrait<AllocType>::WILL_ADDR_MOVE == false`; its allocator pointer must outlive the container.
- For manual heap construction, pair `create_obj<T>` with `destroy_obj<T>` and `create_array<T>` with `destroy_array<T>`. These helpers run destructors; raw `deallocate` only releases storage.

Read [references/allocator-selection.md](references/allocator-selection.md) when selecting a concrete pool, aligned, atomic, or arena allocator, or when working with `MemoryChunk` subranges. Verify the chosen type's current `AllocatorTrait` and constructor contract in its header.

## Work and validate

Trace who owns each object, who owns the allocator, and whether an ID can move before changing allocation code. Document copy and move behavior for allocator-aware resource types. Test destruction order, moved-from/null handles, allocator emptiness, and any expected allocation failure where the changed contract depends on them. Relevant suites are under `tests/unit/core/allocator_test/` and `tests/unit/core/memory_test/`.

Use `python validation.py -t debug -- --gtest_filter='RelevantSuite*'` for focused validation; the wrapper then runs the full unit suite. Do not include lint in validation.
