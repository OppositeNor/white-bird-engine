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
HISTOGRAM_GARBAGE = 0xDEADBEEF

KernelLoader = Callable[[str, dict[str, str | int]], spy.ComputeKernel]


@pytest.fixture
def rng() -> np.random.Generator:
    return np.random.default_rng(RANDOM_SEED)


def run_histogram(p_device: spy.Device, p_load_kernel: KernelLoader, p_element_type: str, p_data: np.ndarray,
        p_data_start: int, p_data_count: int, p_workgroup_size: int, p_bit_region: int) -> np.ndarray:
    kernel = p_load_kernel("radix_sort_histogram.slang", {"ELEMENT_TYPE": p_element_type,
        "KERNEL_WORKGROUP_SIZE": p_workgroup_size, "DATA_START": p_data_start, "DATA_COUNT": p_data_count,
        "WBE_RADIX_HISTOGRAM_BIT_REGION": p_bit_region, "HISTOGRAM_GARBAGE": HISTOGRAM_GARBAGE})
    usage = spy.BufferUsage.shader_resource | spy.BufferUsage.unordered_access
    data_buffer = p_device.create_buffer(element_count=len(p_data), struct_size=4, usage=usage, data=p_data)
    histogram_buffer = p_device.create_buffer(element_count=BUCKET_SIZE, struct_size=4, usage=usage,
        data=np.zeros(BUCKET_SIZE, dtype=np.uint32))
    kernel.dispatch(thread_count=[p_workgroup_size, 1, 1], vars={"data": data_buffer, "histogram_output": histogram_buffer})
    return histogram_buffer.to_numpy().view(np.uint32)


def histogram_reference(p_data: np.ndarray, p_data_start: int, p_data_count: int, p_bit_region: int) -> np.ndarray:
    section = p_data[p_data_start:p_data_start + p_data_count].view(np.uint32)
    return np.bincount((section >> np.uint32(p_bit_region)) & np.uint32(BUCKET_MASK), minlength=BUCKET_SIZE)


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
    # Elements outside the counted range are kept so that reading past either end changes the histogram.
    data = rng.integers(0, 2 ** 32, p_data_start + p_data_count + 64, dtype=np.uint32).view(p_numpy_type)
    result = run_histogram(device, load_kernel, p_element_type, data, p_data_start, p_data_count, p_workgroup_size,
        p_bit_region)
    np.testing.assert_array_equal(result, histogram_reference(data, p_data_start, p_data_count, p_bit_region))


@pytest.mark.parametrize(("p_data_count", "p_workgroup_size"), [(37, 64), (1000, 64)])
def test_workgroup_scope_radix_sort_histogram_single_bucket(device: spy.Device, load_kernel: KernelLoader,
        p_data_count: int, p_workgroup_size: int) -> None:
    # Every invocation increments the same counter, so lost atomic updates show up as a short count.
    data = np.full(p_data_count, 0xAB << 8, dtype=np.uint32)
    result = run_histogram(device, load_kernel, "uint", data, 0, p_data_count, p_workgroup_size, 8)
    expected = np.zeros(BUCKET_SIZE, dtype=np.uint32)
    expected[0xAB] = p_data_count
    np.testing.assert_array_equal(result, expected)
