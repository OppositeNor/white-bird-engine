# Pass-published octree construction

`oct_tree_2` keeps the original octree's 12-byte node layout, leaf layout, packed
child indices, and debug shaders. `OctTreeRenderFeature` owns the resources and
render tasks for both implementations in the deferred rendering executable.

Select an implementation at startup (the default is `2`):

```sh
WBE_OCT_TREE_IMPLEMENTATION=2 ./build/debug/bin/wbe_render_test_vulkan_draw_sponza_deferred_test
WBE_OCT_TREE_IMPLEMENTATION=1 ./build/debug/bin/wbe_render_test_vulkan_draw_sponza_deferred_test
```

In the executable, `V` cycles through normal rendering, completed-tree drawing,
and the pre-deferred view. `R` switches shadow graphs. `F1` captures the current
view under `test_env/captures/`. The shared `WBE_CAPTURE_PNG` and
`WBE_CAPTURE_FRAME` environment variables also work.

## Publication contract

Construction reads `OctTreeNode.children_data` with ordinary loads. Descriptions
of reachable nodes remain unchanged for the whole raster or compute pass.
Allocation claims and leaf occupancy updates operate atomically on the separate
`oct_tree_pending_children` buffer. A successful allocator initializes all eight
children, writes the pending block description, and defers its own traversal too.

The pending description retains state `ALLOCATING`, the existing 22-bit block
index, and the eight occupancy bits. Zero occupancy identifies a node block;
leaf allocation seeds the creating voxel's occupancy bit before finishing the
pending description. Failed reservations restore `EMPTY` and report the existing
shader error.

Node- and leaf-block reservation use one atomic add on `node_count` or
`leaves_count`, followed by a bounds check. An invalid ticket is rolled back with
a second atomic add before reporting overflow; it never initializes child storage.
This retains the configured buffer capacities without compare-exchange retry
loops. Either count may temporarily exceed capacity inside construction, so it
must not be used as a buffer bound until the
pass has finished. Construction does not free successful reservations, and each
voxel has at most one outstanding ticket, keeping rollback safe and transient
counts bounded under the test's configured limits.

The state-update dispatch publishes pending descriptions with ordinary stores.
It also publishes occupancy changes on existing leaves. No construction pass may
run concurrently with this dispatch. Explicit graph dependencies synchronize the
raster passes with the first update, and `RTRunComputePipelinesSequentialVK`
provides storage-buffer memory barriers between subsequent dispatches:

```text
reset → opaque raster → double-sided raster → update states → debug capture
      → (reset destination count → deferred propagation → update states) × max_depth
      → consumers
```

Each construction round reaches at most one newly allocated level. The current
iteration count includes consuming the final leaf retries and draining both
queues. The final update runs even when propagation has no remaining work.

## Deferred queue bound

The initial raster pass atomically claims each final voxel in
`oct_tree_voxel_claims`. Only its first triangle hit starts a retry chain. This is
valid because construction currently records occupancy only; adding per-hit
material or lighting accumulation would require revisiting this deduplication.
Deferred passes do not recheck the claims.

At the test's depth seven, there are at most 2,097,152 retry chains. Each retry
emits at most one successor. The configured deferred capacity is 1,048,576 entries;
scenes exceeding that capacity report a shader error. The claim bitset occupies
256 KiB and is cleared each frame. The pending buffer uses four bytes per
node-capacity slot. Node and leaf capacity failures also remain shader errors.
The class statically bounds transient node and leaf reservations to prevent counter wrap.

This trades per-node traversal atomics and barriers for additional publication
dispatches and buffer traffic. Measure the full construction sequence when
comparing GPU performance.
