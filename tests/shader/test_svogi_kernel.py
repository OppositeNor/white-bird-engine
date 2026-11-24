# Copyright 2026 OppositeNor
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
from typing import Callable

import numpy as np
import pytest
import slangpy as spy

from test_svo_common import LEAF_DTYPE, STATE_BIT_OFFSET, STATE_LEAF, StaticSVO, build_static_svo, make_buffer, random_voxels
from test_svogi_utils import (UNWRITTEN, add_unallocated_leaf_block, injection_leaves, make_static_tree_info,
    occupancy_count, read_occupancy_buffer, supersede_leaf_block)


RANDOM_SEED = 20261124

KernelFunctionLoader = Callable[[str, str, dict[str, str | int]], spy.ComputeKernel]

KERNEL_SOURCE = "svogi/svogi_kernel.slang"
# The workgroup size of inject_svo_occupancy().
INJECTION_WORKGROUP_SIZE = 256


@pytest.fixture
def rng() -> np.random.Generator:
    return np.random.default_rng(RANDOM_SEED)


def svo_info(tree_info: np.ndarray) -> dict[str, object]:
    """The SVOInfo constant buffer of a tree info record."""
    record = tree_info[0]
    return {name: (record[name].tolist() if name == "origin" else record[name].item()) for name in tree_info.dtype.names}


def run_occupancy_injection(device: spy.Device, load_kernel_function: KernelFunctionLoader, nodes: np.ndarray,
        leaves: np.ndarray, tree_info: np.ndarray, element_count: int, workgroup_count: int) -> np.ndarray:
    occupancy = make_buffer(device, np.full(element_count, UNWRITTEN, dtype=np.float32))
    load_kernel_function(KERNEL_SOURCE, "inject_svo_occupancy", {}).dispatch(
        thread_count=[workgroup_count * INJECTION_WORKGROUP_SIZE, 1, 1], vars={
        "svo_info": svo_info(tree_info), "tree_nodes": make_buffer(device, nodes),
        "tree_leaves": make_buffer(device, leaves), "atlas": occupancy, "frame_slot": 0})
    return read_occupancy_buffer(occupancy, element_count)


def expected_injection(tree: StaticSVO, nodes: np.ndarray, leaves: np.ndarray, element_count: int) -> np.ndarray:
    """Every occupied static leaf holds 1, and every leaf parent the fraction of its occupied sectors."""
    expected = np.full(element_count, UNWRITTEN, dtype=np.float32)
    for leaf_index in tree.leaf_of_voxel.values():
        expected[leaves[leaf_index]["data_index"]] = 1.0
    for node_index, (children_data, _, _) in enumerate(tree.nodes):
        if children_data >> STATE_BIT_OFFSET == STATE_LEAF:
            expected[nodes[node_index]["data_index"]] = occupancy_count(children_data) / 8.0
    return expected


@pytest.mark.parametrize("max_depth", [2, 4])
def test_inject_svo_occupancy_injects_leaves_and_their_parents(device: spy.Device,
        load_kernel_function: KernelFunctionLoader, rng: np.random.Generator, max_depth: int) -> None:
    voxels = random_voxels(rng, 400, max_depth)
    tree = build_static_svo(voxels, max_depth)
    nodes = tree.node_array()
    leaves = injection_leaves(tree, np.zeros(0, dtype=LEAF_DTYPE), len(nodes))
    # A block past the allocated leaves, which only the SVO's totals keep from being injected.
    nodes, leaves = add_unallocated_leaf_block(nodes, leaves)
    element_count = len(nodes) + len(leaves) + 1
    # One workgroup more than the leaves need: its invocations lie past every leaf and block.
    workgroup_count = -(-len(leaves) // INJECTION_WORKGROUP_SIZE) + 1
    result = run_occupancy_injection(device, load_kernel_function, nodes, leaves, make_static_tree_info(tree),
        element_count, workgroup_count)
    np.testing.assert_array_equal(result, expected_injection(tree, nodes, leaves, element_count))


def test_inject_svo_occupancy_injects_dynamic_blocks_in_place_of_superseded_ones(device: spy.Device,
        load_kernel_function: KernelFunctionLoader, rng: np.random.Generator) -> None:
    max_depth = 3
    voxels = random_voxels(rng, 100, max_depth)
    tree = build_static_svo(voxels, max_depth)
    dynamic_leaves, parent, static_block = supersede_leaf_block(tree, voxels[0])
    nodes = tree.node_array(len(tree.nodes) + 8)
    leaves = injection_leaves(tree, dynamic_leaves, len(nodes))
    tree_info = make_static_tree_info(tree)
    tree_info["dynamic_node_count"] = 8
    tree_info["dynamic_leaves_count"] = 8
    element_count = len(nodes) + len(leaves)
    result = run_occupancy_injection(device, load_kernel_function, nodes, leaves, tree_info, element_count,
        -(-len(leaves) // INJECTION_WORKGROUP_SIZE))
    expected = expected_injection(tree, nodes, leaves, element_count)
    # The superseded static block writes nothing; its parent now owns the dynamic block, whose only occupied leaf is the
    # moved voxel's.
    for offset in range(8):
        expected[leaves[static_block + offset]["data_index"]] = UNWRITTEN
    expected[leaves[len(tree.leaves) + tree.leaf_of_voxel[voxels[0]] % 8]["data_index"]] = 1.0
    expected[nodes[parent]["data_index"]] = 1.0 / 8.0
    np.testing.assert_array_equal(result, expected)
