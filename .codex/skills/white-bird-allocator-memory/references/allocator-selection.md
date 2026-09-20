# Allocator selection details

Use this guide when a task needs a concrete allocator beyond the default or needs to partition backing storage. Check the referenced header for constructor arguments and current traits before editing code.

| Need | Type and header | Contract that affects usage |
| --- | --- | --- |
| General standalone heap | `HeapAllocatorRAM` in `core/allocator/heap_allocator_ram.hh` | Allocates through RAM and tracks outstanding IDs. Useful in isolated tests. |
| Aligned standalone heap | `HeapAllocatorAlignedRAM` in `core/allocator/heap_allocator_aligned_ram.hh` | Alignment must satisfy its API; allocated blocks are tracked. |
| Finite variable-size pool | `HeapAllocatorPool` in `core/allocator/heap_allocator_pool.hh` | Backed by a `MemoryChunk`; space is finite and free blocks coalesce. |
| Finite fixed-size pool | `HeapAllocatorFixedSizePool` in `core/allocator/heap_allocator_fixed_size_pool.hh` | IDs can move (`WILL_ADDR_MOVE=true`); do not use as an `STLAllocator` backend or assume an ID is an address. |
| Finite aligned pool | `HeapAllocatorAlignedPool` or `HeapAllocatorAlignedPoolImplicitList` in their matching headers | Use the supported alignment and capacity. Their `get()` paths assert membership in debug builds. |
| Shared concurrent pool | `HeapAllocatorAtomicMutexAlignedPoolImplicitList`, `HeapAllocatorAtomicSharedMutexAlignedPoolImplicitList`, or `HeapAllocatorAtomicSharedMutexAlignedPool` | Synchronize allocator operations; object access and lifetime still need the caller's threading contract. |
| Arena allocation | `HeapAllocatorAtomicArenaAlignedPoolImplicitList` or `HeapAllocatorAtomicThreadLocalArenaAlignedPoolImplicitList` | One backing chunk is split into fixed arenas; use `allocate_in_arena` only with a valid arena index. |

`HeapAllocatorDefault` in `core/core_utils.hh` aliases `HeapAllocatorAtomicArenaAlignedPoolImplicitList`. Rendering uses a separate `RenderAllocator` alias in `function/render/render_defs.hh`; preserve that subsystem's allocator choice when working in render code.

`MemoryChunk` in `core/allocator/memory_chunk.hh` is a reference-counted handle to continuous backing storage. Pool and stack constructors can use a checked `(chunk, start_offset, size)` subrange. Keep the backing storage alive for the allocator and its users; do not overlap live allocator ranges unintentionally.

Use `allocate()` for required allocations and `try_allocate()` only when allocation failure is an expected recoverable result. `IAllocator::get()` resolves a valid ID; `try_get()` is the explicit path used by some pools when probing without debug membership checks. Neither makes a dangling ID safe to use.
