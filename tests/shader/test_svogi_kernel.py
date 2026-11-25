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

from test_svo_common import (LEAF_DTYPE, STATE_BIT_OFFSET, STATE_LEAF, StaticSVO, Voxel, build_static_svo, make_buffer,
    random_voxels)
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


# The workgroup extent of inject_svogi_radiance(), one invocation per shadow-map UV.
RADIANCE_WORKGROUP_SIZE = 16
# The SVO the radiance tests inject into: a unit tree scaled to [0, SVO_WIDTH) cubed at the origin.
RADIANCE_SVO_WIDTH = 8.0
RADIANCE_MAX_DEPTH = 2
# Sub-voxels per leaf, the stride of a leaf's data index in a radiance key.
SUBVOXEL_COUNT = 27
# Words of one GPU vector element: a (key, local index) pair, and a SegmentedWrapper<float3x3> (9 floats and a break).
INDEX_WORDS = 2
STAGED_WORDS = 10
GPU_VECTOR_HEADER_WORDS = 4
SENTINEL = 0xFFFFFFFF
# The flux of the light inside the SVO; powers of two, so the radiance rows are exact multiples of one another.
INSIDE_FLUX = (0.25, 0.5, 1.0)
LIGHT_POSITION = (4.0, 4.0, 4.0)


def make_gpu_vector(device: spy.Device, word_count: int, size: int, capacity: int) -> spy.Buffer:
    """A GPU vector of capacity elements of word_count words, holding size elements, every element word SENTINEL."""
    data = np.full(GPU_VECTOR_HEADER_WORDS + capacity * word_count, SENTINEL, dtype=np.uint32)
    data[:GPU_VECTOR_HEADER_WORDS] = 0
    data[0] = size * word_count * 4
    usage = spy.BufferUsage.shader_resource | spy.BufferUsage.unordered_access
    return device.create_buffer(element_count=len(data), struct_size=4, usage=usage, data=data)


def read_gpu_vector(buffer: spy.Buffer, word_count: int) -> tuple[int, np.ndarray]:
    """The element count of a GPU vector and all of its capacity's elements, as (capacity, word_count) words."""
    words = buffer.to_numpy().view(np.uint32)
    return int(words[0]) // (word_count * 4), words[GPU_VECTOR_HEADER_WORDS:].reshape(-1, word_count)


def constant_texture(device: spy.Device, format: spy.Format, value: list[float], size: int = 32) -> spy.Texture:
    data = np.tile(np.array(value, dtype=np.float32), (size, size, 1))
    return device.create_texture(format=format, width=size, height=size, usage=spy.TextureUsage.shader_resource,
        data=data)


def make_light_infos(device: spy.Device, kernel: spy.ComputeKernel, ranges: list[float]) -> spy.Buffer:
    """
    The spot light table: one light per range at LIGHT_POSITION, with identity matrices, so the texel at UV (u, v) sees
    along the direction from the light to (2u - 1, 2v - 1, 1), at the range times the shadow depth.
    """
    layout = next(parameter for parameter in kernel.program.layout.parameters if parameter.name == "light_infos")
    element_layout = layout.type_layout.element_type_layout
    usage = spy.BufferUsage.shader_resource | spy.BufferUsage.unordered_access
    buffer = device.create_buffer(size=element_layout.stride, usage=usage)
    cursor = spy.BufferCursor(element_layout, buffer)
    for index, light_range in enumerate(ranges):
        light = cursor[0]["spot_light_info"][index]
        light["view"] = spy.float4x4.identity()
        light["proj"] = spy.float4x4.identity()
        light["inv_view_proj"] = spy.float4x4.identity()
        light["position"] = spy.float3(*LIGHT_POSITION)
        light["direction"] = spy.float3(0.0, 0.0, 1.0)
        light["color"] = spy.float3(1.0, 1.0, 1.0)
        light["range"] = light_range
    cursor[0]["spot_light_count"] = len(ranges)
    cursor.apply()
    return buffer


def run_radiance_injection(device: spy.Device, load_kernel_function: KernelFunctionLoader, voxels: list[Voxel],
        ranges: list[float], workgroup_count: int, index_size: int, staged_size: int,
        capacity: int) -> tuple[StaticSVO, tuple[int, np.ndarray], tuple[int, np.ndarray]]:
    """
    Inject the radiance of one light per range into a static SVO of voxels with a workgroup_count by workgroup_count
    dispatch. Every light's shadow depth is 1; the first light's flux is INSIDE_FLUX and the others' white.
    """
    tree = build_static_svo(voxels, RADIANCE_MAX_DEPTH)
    tree_info = make_static_tree_info(tree)
    info = svo_info(tree_info)
    info["origin"] = [0.0, 0.0, 0.0]
    info["width"] = RADIANCE_SVO_WIDTH
    kernel = load_kernel_function(KERNEL_SOURCE, "inject_svogi_radiance", {})
    moments = constant_texture(device, spy.Format.rg32_float, [1.0, 1.0])
    fluxes = [constant_texture(device, spy.Format.rgba32_float, [*INSIDE_FLUX, 1.0]),
        constant_texture(device, spy.Format.rgba32_float, [1.0, 1.0, 1.0, 1.0])]
    normals = constant_texture(device, spy.Format.rg32_float, [0.0, 0.0])
    sampler = device.create_sampler()
    lights = [{"shadowmap": moments.descriptor_handle_ro, "rsm_flux": fluxes[min(index, 1)].descriptor_handle_ro,
        "rsm_octa_normal": normals.descriptor_handle_ro, "shadowmap_sampler": sampler.descriptor_handle}
        for index in range(4)]
    index_buffer = make_gpu_vector(device, INDEX_WORDS, index_size, capacity)
    staged_buffer = make_gpu_vector(device, STAGED_WORDS, staged_size, capacity)
    thread_count = workgroup_count * RADIANCE_WORKGROUP_SIZE
    kernel.dispatch(thread_count=[thread_count, thread_count, 1], vars={"svo_info": info,
        "tree_nodes": make_buffer(device, tree.node_array()), "tree_leaves": make_buffer(device, tree.leaf_array()),
        "index_buffer": index_buffer, "staged_buffer": staged_buffer,
        "light_infos": make_light_infos(device, kernel, ranges), "frame_slot": 0, "lights": lights})
    return tree, read_gpu_vector(index_buffer, INDEX_WORDS), read_gpu_vector(staged_buffer, STAGED_WORDS)


def all_voxels() -> list[Voxel]:
    side = 1 << RADIANCE_MAX_DEPTH
    return [(x, y, z) for x in range(side) for y in range(side) for z in range(side)]


@pytest.mark.parametrize("workgroup_count", [1, 2])
def test_inject_svogi_radiance_appends_each_located_sample(device: spy.Device,
        load_kernel_function: KernelFunctionLoader, workgroup_count: int) -> None:
    # Every texel of the first light sees a point at distance 1 from the light, inside the fully occupied SVO; the second
    # light's points, at distance 100, lie outside it and are skipped.
    index_size = 3
    sample_count = (workgroup_count * RADIANCE_WORKGROUP_SIZE) ** 2
    tree, (index_count, indices), (staged_count, staged) = run_radiance_injection(device, load_kernel_function,
        all_voxels(), [1.0, 100.0], workgroup_count, index_size, 0, index_size + sample_count + 8)
    assert index_count == index_size + sample_count
    # The staged buffer grows to the index buffer's size, so its entries pair with the indices.
    assert staged_count == index_count
    np.testing.assert_array_equal(indices[:index_size], SENTINEL)
    np.testing.assert_array_equal(staged[:index_size], SENTINEL)
    np.testing.assert_array_equal(indices[index_count:], SENTINEL)
    entries = indices[index_size:index_count]
    radiance = staged[index_size:index_count]
    # Each workgroup appends its samples as one range, each paired with its index in the workgroup's local list; every
    # invocation located one sample.
    np.testing.assert_array_equal(entries[:, 1].reshape(workgroup_count ** 2, -1),
        np.tile(np.arange(RADIANCE_WORKGROUP_SIZE ** 2), (workgroup_count ** 2, 1)))
    # Staged entries start unsegmented.
    np.testing.assert_array_equal(radiance[:, 9], 0)
    rows = np.ascontiguousarray(radiance[:, :9]).view(np.float32).reshape(-1, 3, 3)
    # Row c is the sub-voxel position scaled by the flux's channel c.
    subvoxel_positions = rows[:, 2] / np.float32(INSIDE_FLUX[2])
    np.testing.assert_array_equal(rows[:, 0], subvoxel_positions * np.float32(INSIDE_FLUX[0]))
    np.testing.assert_array_equal(rows[:, 1], subvoxel_positions * np.float32(INSIDE_FLUX[1]))
    # Each key names an occupied leaf's sub-voxel; with the position in that sub-voxel it rebuilds the world position,
    # which lies at the light's range times the shadow depth from the light.
    leaves = tree.leaf_array()
    voxel_of_data_index = {int(leaves[leaf]["data_index"]): voxel for voxel, leaf in tree.leaf_of_voxel.items()}
    keys = entries[:, 0]
    assert set(int(key) // SUBVOXEL_COUNT for key in keys) <= set(voxel_of_data_index)
    subvoxel_indices = keys % SUBVOXEL_COUNT
    subvoxels = np.stack([subvoxel_indices % 3, subvoxel_indices // 3 % 3, subvoxel_indices // 9], axis=1)
    leaf_positions = (subvoxels + subvoxel_positions - 0.5) / 2.0
    assert np.all((leaf_positions >= -1e-6) & (leaf_positions < 1.0 + 1e-6))
    voxel_coordinates = np.array([voxel_of_data_index[int(key) // SUBVOXEL_COUNT] for key in keys], dtype=np.float32)
    leaf_width = RADIANCE_SVO_WIDTH / (1 << RADIANCE_MAX_DEPTH)
    world_positions = (voxel_coordinates + leaf_positions) * leaf_width
    np.testing.assert_allclose(np.linalg.norm(world_positions - np.array(LIGHT_POSITION), axis=1), 1.0, atol=1e-5)


def test_inject_svogi_radiance_keeps_a_larger_staged_size(device: spy.Device,
        load_kernel_function: KernelFunctionLoader) -> None:
    index_size = 5
    staged_size = 600
    sample_count = RADIANCE_WORKGROUP_SIZE ** 2
    _, (index_count, indices), (staged_count, staged) = run_radiance_injection(device, load_kernel_function,
        all_voxels(), [1.0], 1, index_size, staged_size, staged_size + 8)
    assert index_count == index_size + sample_count
    assert staged_count == staged_size
    # The samples are still staged at their indices' positions; the rest of the staged elements are untouched.
    assert np.all(staged[index_size:index_count, 9] == 0)
    np.testing.assert_array_equal(staged[:index_size], SENTINEL)
    np.testing.assert_array_equal(staged[index_count:], SENTINEL)


def test_inject_svogi_radiance_skips_samples_outside_occupied_leaves(device: spy.Device,
        load_kernel_function: KernelFunctionLoader) -> None:
    # The first light's points all lie in voxel (1, 1, 1), within distance 1 of the light's corner at (4, 4, 4); the
    # second light's lie outside the SVO.
    voxels = [voxel for voxel in all_voxels() if voxel != (1, 1, 1)]
    index_size = 7
    _, (index_count, indices), (staged_count, staged) = run_radiance_injection(device, load_kernel_function, voxels,
        [1.0, 100.0], 1, index_size, 2, 64)
    assert index_count == index_size
    assert staged_count == index_size
    np.testing.assert_array_equal(indices, SENTINEL)
    np.testing.assert_array_equal(staged, SENTINEL)
