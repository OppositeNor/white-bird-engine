# Copyright 2025 OppositeNor
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
from pathlib import Path
from typing import Callable

import numpy as np
import pytest
import slangpy as spy

from test_svo_common import (Voxel, LEAF_DTYPE, OCCUPANCY_BIT_OFFSET, STATE_BIT_OFFSET, STATE_EMPTY, STATE_LEAF,
    STATE_LEAF_DYNAMIC, STATE_NODE_DYNAMIC, StaticSVO, build_static_svo, make_buffer, make_tree_info, node_depth,
    pack_children_data, pack_parent_data, random_voxels, read_buffer, voxel_prefix)


RANDOM_SEED = 20261024
KERNEL_DIR = Path(__file__).parent / "kernels"
SAMPLE_COUNT = 1024

# Texels along each axis of an SVO voxel data brick (SVO_BRICK_SIZE).
BRICK_SIZE = 3
# Sub-voxels of one leaf's brick (SVOGI_SUBVOXEL_COUNT), the stride of a leaf's data index in a radiance key.
SUBVOXEL_COUNT = BRICK_SIZE ** 3
# A non-unit SVO placement whose width is a power of two, so the sample positions map to leaf positions exactly.
SVO_ORIGIN = np.array([-3.0, 1.5, 2.0], dtype=np.float32)
SVO_WIDTH = 8.0
# Local leaf coordinates of the samples: a grid offset from the sub-voxel boundaries at 0.25 and 0.75.
LEAF_COORDINATES = (np.arange(16, dtype=np.float32) + 0.5) / 16.0

KernelLoader = Callable[[str, dict[str, str | int]], spy.ComputeKernel]

# Elements nothing is expected to write; not a valid occupancy.
UNWRITTEN = -1.0


@pytest.fixture
def rng() -> np.random.Generator:
    return np.random.default_rng(RANDOM_SEED)


@pytest.fixture(scope="module")
def subvoxel_functions(device: spy.Device) -> spy.Module:
    return spy.Module.load_from_file(device, str(KERNEL_DIR / "svogi_subvoxel_functions.slang"))


def make_occupancy_buffer(p_device: spy.Device, p_values: np.ndarray) -> spy.Buffer:
    return make_buffer(p_device, p_values.astype(np.float32))


def read_occupancy_buffer(p_buffer: spy.Buffer, p_count: int) -> np.ndarray:
    return read_buffer(p_buffer, np.float32)[:p_count]


def run_propagation(p_device: spy.Device, p_load_kernel: KernelLoader, p_nodes: np.ndarray, p_leaves: np.ndarray,
        p_tree_info: np.ndarray, p_occupancy: spy.Buffer, p_child_depth: int) -> None:
    p_load_kernel("svogi_occupancy_propagation.slang", {}).dispatch(thread_count=[-(-len(p_nodes) // 8), 1, 1], vars={
        "nodes": make_buffer(p_device, p_nodes), "leaves": make_buffer(p_device, p_leaves),
        "tree_info": make_buffer(p_device, p_tree_info), "occupancy": p_occupancy, "child_depth": p_child_depth})


def run_injection(p_device: spy.Device, p_load_kernel: KernelLoader, p_nodes: np.ndarray, p_leaves: np.ndarray,
        p_tree_info: np.ndarray, p_occupancy: spy.Buffer, p_inject_parents: bool, p_thread_count: int) -> None:
    p_load_kernel("svogi_occupancy_injection.slang", {"INJECT_PARENTS": int(p_inject_parents)}).dispatch(
        thread_count=[p_thread_count, 1, 1], vars={"nodes": make_buffer(p_device, p_nodes),
        "leaves": make_buffer(p_device, p_leaves), "tree_info": make_buffer(p_device, p_tree_info), "occupancy": p_occupancy,
        "invocation_count": p_thread_count})


def injection_leaves(p_tree: StaticSVO, p_dynamic_leaves: np.ndarray, p_node_count: int) -> np.ndarray:
    """The static leaves followed by p_dynamic_leaves, with data indices past the nodes' so the two never share an element."""
    leaves = np.concatenate([p_tree.leaf_array()[:len(p_tree.leaves)], p_dynamic_leaves])
    leaves["data_index"] = p_node_count + np.arange(len(leaves))
    return leaves


def occupancy_count(p_children_data: int) -> int:
    return bin((p_children_data >> OCCUPANCY_BIT_OFFSET) & 0xFF).count("1")


def supersede_leaf_block(p_tree: StaticSVO, p_voxel: tuple[int, int, int]) -> tuple[np.ndarray, int, int]:
    """
    Move the leaf block of p_voxel's parent into one dynamic block, which follows the static leaves, keeping only that
    voxel occupied; return the dynamic leaves, the parent, and the static block.
    """
    static_leaf = p_tree.leaf_of_voxel[p_voxel]
    parent = p_tree.leaves[static_leaf][1]
    p_tree.nodes[parent][0] = pack_children_data(STATE_LEAF_DYNAMIC, len(p_tree.leaves) // 8, 1 << (static_leaf % 8))
    dynamic_leaves = np.zeros(8, dtype=LEAF_DTYPE)
    dynamic_leaves["parent_index"] = parent
    return dynamic_leaves, parent, static_leaf // 8 * 8


def add_unallocated_leaf_block(p_nodes: np.ndarray, p_leaves: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """
    Append, past the allocated nodes and leaves, a parent whose fully occupied dynamic leaf block follows the leaves, so
    that only the bounds of the SVO's totals keep them from being injected.
    """
    nodes = np.concatenate([p_nodes, np.zeros(1, dtype=p_nodes.dtype)])
    nodes[-1] = (pack_children_data(STATE_LEAF_DYNAMIC, len(p_leaves) // 8, 0xFF), len(nodes) + len(p_leaves) + 8,
        pack_parent_data(0, 2), 0)
    leaves = np.concatenate([p_leaves, np.zeros(8, dtype=LEAF_DTYPE)])
    leaves[-8:]["data_index"] = len(nodes) + len(p_leaves) + np.arange(8)
    leaves[-8:]["parent_index"] = len(nodes) - 1
    return nodes, leaves


@pytest.mark.parametrize("p_max_depth", [2, 4])
def test_inject_svo_leaf_occupancy_writes_occupied_leaves(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator, p_max_depth: int) -> None:
    voxels = random_voxels(rng, 40, p_max_depth)
    tree = build_static_svo(voxels, p_max_depth)
    nodes = tree.node_array()
    leaves = injection_leaves(tree, np.zeros(0, dtype=LEAF_DTYPE), len(nodes))
    nodes, leaves = add_unallocated_leaf_block(nodes, leaves)
    occupancy = make_occupancy_buffer(device, np.full(len(nodes) + len(leaves) + 1, UNWRITTEN, dtype=np.float32))
    # Threads past the last allocated leaf write nothing.
    run_injection(device, load_kernel, nodes, leaves, make_static_tree_info(tree), occupancy, False, len(leaves) + 13)
    expected = np.full(len(nodes) + len(leaves) + 1, UNWRITTEN, dtype=np.float32)
    for leaf_index in tree.leaf_of_voxel.values():
        expected[leaves[leaf_index]["data_index"]] = 1.0
    np.testing.assert_array_equal(read_occupancy_buffer(occupancy, len(expected)), expected)


def test_inject_svo_leaf_occupancy_skips_superseded_leaves(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator) -> None:
    max_depth = 3
    voxels = random_voxels(rng, 20, max_depth)
    tree = build_static_svo(voxels, max_depth)
    dynamic_leaves, _, static_block = supersede_leaf_block(tree, voxels[0])
    nodes = tree.node_array(len(tree.nodes) + 8)
    leaves = injection_leaves(tree, dynamic_leaves, len(nodes))
    tree_info = make_static_tree_info(tree)
    tree_info["dynamic_node_count"] = 8
    tree_info["dynamic_leaves_count"] = 8
    occupancy = make_occupancy_buffer(device, np.full(len(nodes) + len(leaves), UNWRITTEN, dtype=np.float32))
    run_injection(device, load_kernel, nodes, leaves, tree_info, occupancy, False, len(leaves))
    expected = np.full(len(nodes) + len(leaves), UNWRITTEN, dtype=np.float32)
    for leaf_index in tree.leaf_of_voxel.values():
        if leaf_index // 8 * 8 != static_block:
            expected[leaves[leaf_index]["data_index"]] = 1.0
    # Only the moved voxel's dynamic leaf is written in place of its superseded static block.
    expected[leaves[len(tree.leaves) + tree.leaf_of_voxel[voxels[0]] % 8]["data_index"]] = 1.0
    np.testing.assert_array_equal(read_occupancy_buffer(occupancy, len(expected)), expected)


@pytest.mark.parametrize("p_max_depth", [2, 4])
def test_inject_svo_leaf_parent_occupancy_writes_occupied_fraction(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator, p_max_depth: int) -> None:
    voxels = random_voxels(rng, 40, p_max_depth)
    tree = build_static_svo(voxels, p_max_depth)
    nodes = tree.node_array()
    leaves = injection_leaves(tree, np.zeros(0, dtype=LEAF_DTYPE), len(nodes))
    nodes, leaves = add_unallocated_leaf_block(nodes, leaves)
    occupancy = make_occupancy_buffer(device, np.full(len(nodes) + len(leaves) + 1, UNWRITTEN, dtype=np.float32))
    # Threads past the last allocated block write nothing.
    run_injection(device, load_kernel, nodes, leaves, make_static_tree_info(tree), occupancy, True, len(leaves) // 8 + 5)
    expected = np.full(len(nodes) + len(leaves) + 1, UNWRITTEN, dtype=np.float32)
    for node_index, (children_data, data_index, _) in enumerate(tree.nodes):
        if children_data >> STATE_BIT_OFFSET == STATE_LEAF:
            expected[data_index] = occupancy_count(children_data) / 8.0
    np.testing.assert_array_equal(read_occupancy_buffer(occupancy, len(expected)), expected)


def test_inject_svo_leaf_parent_occupancy_skips_superseded_blocks(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator) -> None:
    max_depth = 3
    voxels = random_voxels(rng, 20, max_depth)
    tree = build_static_svo(voxels, max_depth)
    dynamic_leaves, parent, _ = supersede_leaf_block(tree, voxels[0])
    nodes = tree.node_array(len(tree.nodes) + 8)
    leaves = injection_leaves(tree, dynamic_leaves, len(nodes))
    tree_info = make_static_tree_info(tree)
    tree_info["dynamic_node_count"] = 8
    tree_info["dynamic_leaves_count"] = 8
    occupancy = make_occupancy_buffer(device, np.full(len(nodes) + len(leaves), UNWRITTEN, dtype=np.float32))
    # Inject only the static blocks: the superseded one must not write the parent its dynamic block now owns.
    run_injection(device, load_kernel, nodes, leaves, tree_info, occupancy, True, len(tree.leaves) // 8)
    result = read_occupancy_buffer(occupancy, len(nodes) + len(leaves))
    assert result[nodes[parent]["data_index"]] == UNWRITTEN
    # The dynamic block writes it.
    run_injection(device, load_kernel, nodes, leaves, tree_info, occupancy, True, len(leaves) // 8)
    result = read_occupancy_buffer(occupancy, len(nodes) + len(leaves))
    assert result[nodes[parent]["data_index"]] == 1.0 / 8.0


def node_depths(p_tree: StaticSVO) -> dict[int, int]:
    return {node_index: depth for (depth, _), node_index in p_tree.node_of_prefix.items()}


def expected_occupancy(p_tree: StaticSVO, p_voxels: list[tuple[int, int, int]]) -> np.ndarray:
    """Occupancy of every node with children: the fraction of the leaves below it that are occupied."""
    result = np.full(len(p_tree.nodes), UNWRITTEN, dtype=np.float32)
    for (depth, prefix), node_index in p_tree.node_of_prefix.items():
        if p_tree.nodes[node_index][0] >> STATE_BIT_OFFSET == STATE_EMPTY:
            continue
        count = sum(1 for voxel in p_voxels if voxel_prefix(voxel, depth, p_tree.max_depth) == prefix)
        result[node_index] = count / 8.0 ** (p_tree.max_depth - depth)
    return result


def make_static_tree_info(p_tree: StaticSVO) -> np.ndarray:
    tree_info = make_tree_info(p_tree.max_depth)
    tree_info["node_count"] = len(p_tree.nodes)
    tree_info["leaves_count"] = len(p_tree.leaves)
    return tree_info


@pytest.mark.parametrize("p_max_depth", [3, 4, 5])
def test_propagate_svo_occupancy_writes_one_level_per_dispatch(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator, p_max_depth: int) -> None:
    voxels = random_voxels(rng, 60, p_max_depth)
    tree = build_static_svo(voxels, p_max_depth)
    expected = expected_occupancy(tree, voxels)
    depths = node_depths(tree)
    # The injection has written the parents of the leaves.
    initial = np.full(len(tree.nodes), UNWRITTEN, dtype=np.float32)
    for node_index, depth in depths.items():
        if depth == p_max_depth - 1:
            initial[node_index] = expected[node_index]
    occupancy = make_occupancy_buffer(device, initial)
    nodes = tree.node_array()
    for child_depth in range(p_max_depth - 1, 1, -1):
        run_propagation(device, load_kernel, nodes, tree.leaf_array(), make_static_tree_info(tree), occupancy, child_depth)
        # Each dispatch writes only the level above its children; shallower levels are still unwritten.
        written = np.array([UNWRITTEN if depths[i] < child_depth - 1 else expected[i] for i in range(len(tree.nodes))],
            dtype=np.float32)
        np.testing.assert_allclose(read_occupancy_buffer(occupancy, len(tree.nodes)), written, rtol=1e-6)
    # Every root sector with voxels below it has its fraction of occupied leaves.
    np.testing.assert_allclose(read_occupancy_buffer(occupancy, len(tree.nodes)), expected, rtol=1e-6)


def test_propagate_svo_occupancy_reads_the_referenced_dynamic_block(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator) -> None:
    max_depth = 3
    voxels = random_voxels(rng, 20, max_depth)
    tree = build_static_svo(voxels, max_depth)
    expected = expected_occupancy(tree, voxels)
    # Replace the children of the first voxel's root sector with a dynamic block, which follows the static nodes, whose
    # only node with children is its first one.
    sector = tree.node_of_prefix[(1, voxel_prefix(voxels[0], 1, max_depth))]
    dynamic_block = len(tree.nodes)
    nodes = tree.node_array(dynamic_block + 8)
    nodes[sector]["children_data"] = pack_children_data(STATE_NODE_DYNAMIC, dynamic_block // 8)
    nodes[dynamic_block:]["data_index"] = np.arange(dynamic_block, dynamic_block + 8)
    nodes[dynamic_block:]["parent_data"] = pack_parent_data(sector, node_depth(int(nodes[sector]["parent_data"])) + 1)
    nodes[dynamic_block]["children_data"] = pack_children_data(STATE_LEAF, 0, 1)
    tree_info = make_static_tree_info(tree)
    tree_info["dynamic_node_count"] = 8
    # The injection has written the parents of the leaves, including those of the superseded block.
    initial = np.full(dynamic_block + 8, UNWRITTEN, dtype=np.float32)
    for node_index in range(len(tree.nodes)):
        if tree.nodes[node_index][0] >> STATE_BIT_OFFSET == STATE_LEAF:
            initial[node_index] = expected[node_index]
    initial[dynamic_block] = 0.5
    occupancy = make_occupancy_buffer(device, initial)
    run_propagation(device, load_kernel, nodes, tree.leaf_array(), tree_info, occupancy, 2)
    result = read_occupancy_buffer(occupancy, dynamic_block + 8)
    # The superseded static block does not write its former parent.
    assert result[sector] == pytest.approx(0.5 / 8.0)
    # The other root sectors still read their static blocks.
    for other_sector in range(8):
        if other_sector != sector:
            assert result[other_sector] == pytest.approx(expected[other_sector], rel=1e-6)


def test_propagate_svo_occupancy_skips_root_sectors_and_absent_depths(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator) -> None:
    max_depth = 4
    voxels = random_voxels(rng, 40, max_depth)
    tree = build_static_svo(voxels, max_depth)
    expected = expected_occupancy(tree, voxels)
    depths = node_depths(tree)
    # Every level below the root sectors already holds its occupancy; the root sectors are unwritten.
    initial = np.array([UNWRITTEN if depths[i] == 1 else expected[i] for i in range(len(tree.nodes))], dtype=np.float32)
    occupancy = make_occupancy_buffer(device, initial)
    nodes = tree.node_array()
    # The root sectors have no stored parent, and no node lies at the leaves' depth, so neither dispatch writes.
    for child_depth in [1, max_depth]:
        run_propagation(device, load_kernel, nodes, tree.leaf_array(), make_static_tree_info(tree), occupancy, child_depth)
        np.testing.assert_array_equal(read_occupancy_buffer(occupancy, len(tree.nodes)), initial)


def subvoxel_reference(p_positions: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """The (position, index) svo_pos_to_subvoxel_pos() returns for each local position."""
    brick_positions = p_positions.astype(np.float32) * np.float32(BRICK_SIZE - 1)
    subvoxels = np.minimum((brick_positions + np.float32(0.5)).astype(np.uint32), BRICK_SIZE - 1)
    indices = subvoxels[:, 0] + BRICK_SIZE * (subvoxels[:, 1] + BRICK_SIZE * subvoxels[:, 2])
    return brick_positions - subvoxels.astype(np.float32) + np.float32(0.5), indices


def call_subvoxel_position(p_functions: spy.Module, p_positions: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    result = p_functions.svogi_subvoxel_position(p_positions.astype(np.float32), _result="numpy")
    return result[:, :3], result[:, 3].astype(np.uint32)


def test_svo_pos_to_subvoxel_pos_matches_reference(subvoxel_functions: spy.Module, rng: np.random.Generator) -> None:
    positions = rng.random((SAMPLE_COUNT, 3), dtype=np.float32)
    subvoxel_positions, indices = call_subvoxel_position(subvoxel_functions, positions)
    expected_positions, expected_indices = subvoxel_reference(positions)
    np.testing.assert_array_equal(indices, expected_indices)
    np.testing.assert_allclose(subvoxel_positions, expected_positions, atol=1e-6)
    # The sub-voxel and the position inside it recover the local position.
    subvoxels = np.stack([indices % BRICK_SIZE, indices // BRICK_SIZE % BRICK_SIZE, indices // BRICK_SIZE ** 2], axis=1)
    np.testing.assert_allclose((subvoxels + subvoxel_positions - 0.5) / (BRICK_SIZE - 1), positions, atol=1e-6)


@pytest.mark.parametrize(("p_coordinate", "p_subvoxel", "p_subvoxel_coordinate"), [
    (0.0, 0, 0.5),
    (0.2, 0, 0.9),
    (0.25, 1, 0.0),
    (0.5, 1, 0.5),
    (0.7, 1, 0.9),
    (0.75, 2, 0.0),
    (0.95, 2, 0.4),
])
def test_svo_pos_to_subvoxel_pos_selects_nearest_subvoxel_center(subvoxel_functions: spy.Module, p_coordinate: float,
        p_subvoxel: int, p_subvoxel_coordinate: float) -> None:
    # The same coordinate on every axis selects the same sub-voxel on each.
    subvoxel_positions, indices = call_subvoxel_position(subvoxel_functions, np.full((1, 3), p_coordinate))
    assert indices[0] == p_subvoxel * (1 + BRICK_SIZE + BRICK_SIZE ** 2)
    np.testing.assert_allclose(subvoxel_positions[0], np.full(3, p_subvoxel_coordinate), atol=1e-6)


def test_svo_pos_to_subvoxel_pos_flattens_x_fastest(subvoxel_functions: spy.Module) -> None:
    positions = np.array([[0.9, 0.0, 0.0], [0.0, 0.9, 0.0], [0.0, 0.0, 0.9], [0.5, 0.5, 0.5], [0.9, 0.5, 0.0]])
    _, indices = call_subvoxel_position(subvoxel_functions, positions)
    np.testing.assert_array_equal(indices, [2, 6, 18, 13, 5])


def test_svo_pos_to_subvoxel_pos_edge_subvoxels_extend_past_the_voxel(subvoxel_functions: spy.Module,
        rng: np.random.Generator) -> None:
    positions = rng.random((SAMPLE_COUNT, 3), dtype=np.float32)
    subvoxel_positions, indices = call_subvoxel_position(subvoxel_functions, positions)
    subvoxels = np.stack([indices % BRICK_SIZE, indices // BRICK_SIZE % BRICK_SIZE, indices // BRICK_SIZE ** 2], axis=1)
    # The low sub-voxel covers only its upper half, the high one only its lower half, and the middle one all of itself.
    low = subvoxel_positions[subvoxels == 0]
    middle = subvoxel_positions[subvoxels == 1]
    high = subvoxel_positions[subvoxels == BRICK_SIZE - 1]
    assert np.all((low >= 0.5) & (low <= 1.0))
    assert np.all((middle >= 0.0) & (middle <= 1.0))
    assert np.all((high >= 0.0) & (high < 0.5))


def run_radiance_sample(p_device: spy.Device, p_load_kernel: KernelLoader, p_tree: StaticSVO, p_positions: np.ndarray,
        p_fluxes: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Locate and weight every flux sample; return the (located, key) results, sub-voxel positions, and radiance."""
    count = len(p_positions)
    tree_info = make_static_tree_info(p_tree)
    tree_info["origin"] = SVO_ORIGIN
    tree_info["width"] = SVO_WIDTH
    padded_positions = np.zeros((count, 4), dtype=np.float32)
    padded_positions[:, :3] = p_positions
    padded_fluxes = np.zeros((count, 4), dtype=np.float32)
    padded_fluxes[:, :3] = p_fluxes
    results = make_buffer(p_device, np.full((count, 2), 0xDEADBEEF, dtype=np.uint32))
    subvoxel_positions = make_buffer(p_device, np.full((count, 4), UNWRITTEN, dtype=np.float32))
    radiance_rows = make_buffer(p_device, np.full((count * 3, 4), UNWRITTEN, dtype=np.float32))
    p_load_kernel("svogi_radiance_sample.slang", {}).dispatch(thread_count=[count, 1, 1], vars={
        "nodes": make_buffer(p_device, p_tree.node_array()), "leaves": make_buffer(p_device, p_tree.leaf_array()),
        "tree_info": make_buffer(p_device, tree_info), "positions": make_buffer(p_device, padded_positions),
        "fluxes": make_buffer(p_device, padded_fluxes), "results": results, "subvoxel_positions": subvoxel_positions,
        "radiance_rows": radiance_rows})
    return (read_buffer(results, np.uint32).reshape(-1, 2)[:count],
        read_buffer(subvoxel_positions, np.float32).reshape(-1, 4)[:count, :3],
        read_buffer(radiance_rows, np.float32).reshape(-1, 3, 4)[:count, :, :3])


def leaf_samples(p_rng: np.random.Generator, p_voxels: list[Voxel], p_max_depth: int) -> tuple[np.ndarray, np.ndarray]:
    """A world position inside each voxel, at a random grid point of LEAF_COORDINATES, and that local position."""
    local_positions = p_rng.choice(LEAF_COORDINATES, (len(p_voxels), 3))
    leaf_width = SVO_WIDTH / float(1 << p_max_depth)
    positions = SVO_ORIGIN + (np.array(p_voxels, dtype=np.float32) + local_positions) * np.float32(leaf_width)
    return positions.astype(np.float32), local_positions


@pytest.mark.parametrize("p_max_depth", [2, 3, 4])
def test_try_locate_svogi_subvoxel_keys_occupied_leaf_subvoxels(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator, p_max_depth: int) -> None:
    occupied = random_voxels(rng, 40, p_max_depth)
    tree = build_static_svo(occupied, p_max_depth)
    # Several samples per occupied voxel, and as many in voxels that are not.
    queries = occupied * 4 + random_voxels(rng, 4 * len(occupied), p_max_depth)
    positions, local_positions = leaf_samples(rng, queries, p_max_depth)
    fluxes = rng.random((len(queries), 3), dtype=np.float32)
    results, subvoxel_positions, radiance = run_radiance_sample(device, load_kernel, tree, positions, fluxes)
    expected_subvoxel_positions, subvoxel_indices = subvoxel_reference(local_positions)
    leaves = tree.leaf_array()
    for index, voxel in enumerate(queries):
        if voxel in tree.leaf_of_voxel:
            data_index = int(leaves[tree.leaf_of_voxel[voxel]]["data_index"])
            assert tuple(results[index]) == (1, SUBVOXEL_COUNT * data_index + subvoxel_indices[index])
            np.testing.assert_allclose(subvoxel_positions[index], expected_subvoxel_positions[index], atol=1e-6)
            np.testing.assert_allclose(radiance[index], np.outer(fluxes[index], expected_subvoxel_positions[index]),
                atol=1e-6)
        else:
            # The trace of an unoccupied voxel ends at a node, which receives no radiance.
            assert tuple(results[index]) == (0, 0)
            np.testing.assert_array_equal(subvoxel_positions[index], np.zeros(3))
            np.testing.assert_array_equal(radiance[index], np.zeros((3, 3)))


def test_try_locate_svogi_subvoxel_rejects_positions_outside_the_svo(device: spy.Device, load_kernel: KernelLoader) -> None:
    max_depth = 2
    # Every leaf is occupied, so only the bounds check can reject a position.
    voxels = [(x, y, z) for x in range(4) for y in range(4) for z in range(4)]
    tree = build_static_svo(voxels, max_depth)
    inside = SVO_ORIGIN + np.float32(SVO_WIDTH / 2.0)
    positions = np.array([
        SVO_ORIGIN - np.float32(0.25),
        SVO_ORIGIN + np.float32(SVO_WIDTH),
        inside - np.array([SVO_WIDTH, 0.0, 0.0], dtype=np.float32),
        inside + np.array([0.0, SVO_WIDTH, 0.0], dtype=np.float32),
        inside + np.array([0.0, 0.0, SVO_WIDTH * 0.5], dtype=np.float32),
        # The origin and the center are inside, and located.
        SVO_ORIGIN,
        inside,
    ], dtype=np.float32)
    results, _, _ = run_radiance_sample(device, load_kernel, tree, positions, np.ones((len(positions), 3), np.float32))
    np.testing.assert_array_equal(results[:, 0], [0, 0, 0, 0, 0, 1, 1])
    np.testing.assert_array_equal(results[:5, 1], np.zeros(5))
