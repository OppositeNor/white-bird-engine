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
from typing import Callable

import numpy as np
import pytest
import slangpy as spy


RANDOM_SEED = 20251003
BUCKET_SIZE = 256
BUCKET_MASK = BUCKET_SIZE - 1
# Initial histogram values stay small enough that adding every count cannot overflow a counter.
INITIAL_COUNT_LIMIT = 1 << 20

KernelLoader = Callable[[str, dict[str, str | int]], spy.ComputeKernel]
# A (data start, data count) range of the data buffer.
DataRange = tuple[int, int]


@pytest.fixture
def rng() -> np.random.Generator:
    return np.random.default_rng(RANDOM_SEED)


def run_histogram(p_device: spy.Device, p_load_kernel: KernelLoader, p_element_type: str, p_data: np.ndarray,
        p_initial_histogram: np.ndarray, p_first_range: DataRange, p_second_range: DataRange, p_workgroup_size: int,
        p_bit_region: int) -> np.ndarray:
    kernel = p_load_kernel("radix_sort_histogram.slang", {"ELEMENT_TYPE": p_element_type,
        "KERNEL_WORKGROUP_SIZE": p_workgroup_size, "FIRST_DATA_START": p_first_range[0],
        "FIRST_DATA_COUNT": p_first_range[1], "SECOND_DATA_START": p_second_range[0],
        "SECOND_DATA_COUNT": p_second_range[1], "WBE_RADIX_HISTOGRAM_BIT_REGION": p_bit_region})
    usage = spy.BufferUsage.shader_resource | spy.BufferUsage.unordered_access
    data_buffer = p_device.create_buffer(element_count=len(p_data), struct_size=4, usage=usage, data=p_data)
    histogram_buffer = p_device.create_buffer(element_count=BUCKET_SIZE, struct_size=4, usage=usage,
        data=p_initial_histogram)
    kernel.dispatch(thread_count=[p_workgroup_size, 1, 1], vars={"data": data_buffer, "histogram": histogram_buffer})
    return histogram_buffer.to_numpy().view(np.uint32)


def histogram_reference(p_data: np.ndarray, p_range: DataRange, p_bit_region: int) -> np.ndarray:
    section = p_data[p_range[0]:p_range[0] + p_range[1]].view(np.uint32)
    return np.bincount((section >> np.uint32(p_bit_region)) & np.uint32(BUCKET_MASK),
        minlength=BUCKET_SIZE).astype(np.uint32)


def random_initial_histogram(p_rng: np.random.Generator) -> np.ndarray:
    return p_rng.integers(0, INITIAL_COUNT_LIMIT, BUCKET_SIZE, dtype=np.uint32)


ELEMENT_TYPES = [("uint", np.uint32), ("int", np.int32)]

# (data start, data count, workgroup size, bit region). Counts below the workgroup size take the one-element-per-invocation
# path; the others split the range into contiguous per-invocation sections.
HISTOGRAM_SHAPES = [
    (0, 1, 64, 0),
    (0, 37, 64, 0),
    (0, 64, 64, 8),
    (0, 65, 64, 16),
    (0, 1000, 64, 24),
    (0, 4096, 256, 4),
    (0, 300, 256, 0),
    (0, 100, 32, 0),
    (13, 37, 64, 0),
    (13, 64, 64, 8),
    (7, 1000, 64, 24),
    (100, 4096, 256, 12),
]


@pytest.mark.parametrize(("p_element_type", "p_numpy_type"), ELEMENT_TYPES)
@pytest.mark.parametrize(("p_data_start", "p_data_count", "p_workgroup_size", "p_bit_region"), HISTOGRAM_SHAPES)
def test_workgroup_scope_radix_sort_histogram(device: spy.Device, load_kernel: KernelLoader, rng: np.random.Generator,
        p_element_type: str, p_numpy_type: type, p_data_start: int, p_data_count: int, p_workgroup_size: int,
        p_bit_region: int) -> None:
    # Elements outside the counted range are kept so that reading past either end changes the histogram. The second
    # call counts an empty range and must leave the histogram unchanged.
    data = rng.integers(0, 2 ** 32, p_data_start + p_data_count + 64, dtype=np.uint32).view(p_numpy_type)
    initial = random_initial_histogram(rng)
    data_range = (p_data_start, p_data_count)
    result = run_histogram(device, load_kernel, p_element_type, data, initial, data_range, (0, 0), p_workgroup_size,
        p_bit_region)
    np.testing.assert_array_equal(result, initial + histogram_reference(data, data_range, p_bit_region))


# (first range, second range, workgroup size). The ranges mix both counting paths, and some overlap so that elements
# counted twice must be added twice.
INCREMENTAL_SHAPES = [
    ((0, 37), (37, 1000), 64),
    ((500, 1000), (0, 20), 64),
    ((5, 64), (69, 64), 64),
    ((0, 300), (100, 300), 256),
    ((10, 4096), (10, 4096), 256),
]


@pytest.mark.parametrize(("p_first_range", "p_second_range", "p_workgroup_size"), INCREMENTAL_SHAPES)
def test_workgroup_scope_radix_sort_histogram_incremental(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator, p_first_range: DataRange, p_second_range: DataRange, p_workgroup_size: int) -> None:
    data_end = max(p_first_range[0] + p_first_range[1], p_second_range[0] + p_second_range[1])
    data = rng.integers(0, 2 ** 32, data_end + 64, dtype=np.uint32)
    initial = random_initial_histogram(rng)
    result = run_histogram(device, load_kernel, "uint", data, initial, p_first_range, p_second_range, p_workgroup_size,
        8)
    expected = initial + histogram_reference(data, p_first_range, 8) + histogram_reference(data, p_second_range, 8)
    np.testing.assert_array_equal(result, expected)


@pytest.mark.parametrize(("p_data_count", "p_workgroup_size"), [(37, 64), (1000, 64)])
def test_workgroup_scope_radix_sort_histogram_single_bucket(device: spy.Device, load_kernel: KernelLoader,
        p_data_count: int, p_workgroup_size: int) -> None:
    # Every invocation increments the same counter, so lost atomic updates show up as a short count.
    data = np.full(p_data_count, 0xAB << 8, dtype=np.uint32)
    initial = np.zeros(BUCKET_SIZE, dtype=np.uint32)
    result = run_histogram(device, load_kernel, "uint", data, initial, (0, p_data_count), (0, 0), p_workgroup_size, 8)
    expected = np.zeros(BUCKET_SIZE, dtype=np.uint32)
    expected[0xAB] = p_data_count
    np.testing.assert_array_equal(result, expected)
