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
from dataclasses import dataclass
from typing import Callable

import numpy as np
import pytest
import slangpy as spy


RANDOM_SEED = 20251003
# Must match large_prefix_scan_utils.slang and gpu_vector.slang.
WORKGROUP_SIZE = 64
BLOCK_SIZE = 256
SECTION_NUM = 256
DATA_START = 4
BYTE_SIZE_INDEX = 0
SECTION_TOTALS_START = 0
BLOCK_TOTALS_START = SECTION_NUM
WORD_BYTE_SIZE = 4
# An upper bound of the words per sections buffer element of every tested type, whatever the structured buffer layout:
# SegmentedWrapper<float3x3> takes 16 words under std430.
MAX_SECTION_ELEMENT_WORD_COUNT = 16
# Guard words past the scanned range; they must stay untouched.
GUARD_COUNT = 64
GUARD_VALUE = 0xDEADBEEF

KernelLoader = Callable[[str, dict[str, str | int]], spy.ComputeKernel]


@dataclass(frozen=True)
class ScanElement:
    """An instantiation of the large prefix scan: its element type and operator, and how to make and check elements."""
    # The Slang element type T.
    element_type: str
    # The Slang IOperator<T> the elements are combined with.
    operator: str
    # The 32-bit words of one element in the GPU vector.
    word_count: int
    # Random elements, as an (element count, word_count) array of words.
    generate: Callable[[np.random.Generator, int], np.ndarray]
    # The inclusive scan of the elements, as words.
    expected: Callable[[np.ndarray], np.ndarray]
    # The words of one sections buffer element, or None when its structured buffer layout is not checked.
    section_word_count: int | None
    # The word of a SegmentedWrapper element's break, or None for other elements. A scanned break is only promised to be
    # nonzero when a segment starts at or before the element, so breaks are compared by whether they are nonzero: the
    # first element is never combined, so it keeps its own break.
    break_word: int | None = None

    def canonical(self, p_words: np.ndarray) -> np.ndarray:
        """The words of elements (element count, word_count) with every break reduced to whether it is nonzero."""
        if self.break_word is None:
            return p_words
        result = p_words.copy()
        result[:, self.break_word] = result[:, self.break_word] != 0
        return result


def add_expected(p_words: np.ndarray) -> np.ndarray:
    """The inclusive prefix sum of every uint lane, wrapping modulo 2^32."""
    return np.cumsum(p_words, axis=0, dtype=np.uint32)


def segmented_sums(p_values: np.ndarray, p_breaks: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """The segmented inclusive prefix sum of int64 values (element count, lanes), where a segment starts at every nonzero
    break and at the first element, and the scanned breaks: 1 when a segment starts at or before the element."""
    sums = np.cumsum(p_values, axis=0)
    starts = np.maximum.accumulate(np.where(p_breaks != 0, np.arange(len(p_breaks)), 0))
    earlier = np.where((starts > 0)[:, None], sums[np.maximum(starts - 1, 0)], 0)
    return sums - earlier, (np.cumsum(p_breaks != 0) > 0).astype(np.uint32)


def generate_breaks(p_rng: np.random.Generator, p_count: int, p_break_probability: float) -> np.ndarray:
    """Breaks set with p_break_probability, to any nonzero value, since every nonzero break starts a segment."""
    return np.where(p_rng.random(p_count) < p_break_probability, p_rng.integers(1, 1 << 31, p_count, dtype=np.uint32),
        0).astype(np.uint32)


def segmented_uint_element(p_break_probability: float) -> ScanElement:
    def generate(p_rng: np.random.Generator, p_count: int) -> np.ndarray:
        values = p_rng.integers(0, 2 ** 32, p_count, dtype=np.uint32)
        return np.stack([values, generate_breaks(p_rng, p_count, p_break_probability)], axis=1)

    def expected(p_words: np.ndarray) -> np.ndarray:
        sums, breaks = segmented_sums(p_words[:, :1].astype(np.int64), p_words[:, 1])
        return np.concatenate([(sums & 0xFFFFFFFF).astype(np.uint32), breaks[:, None]], axis=1)

    return ScanElement("SegmentedWrapper<uint>", "SegmentedPrefix<uint, Add<uint>>", 2, generate, expected, 2, 1)


def segmented_float3x3_element(p_break_probability: float) -> ScanElement:
    """The segmented scan SVOGI reduces radiance with. Values are small integers, so every float sum is exact and
    independent of the combination order."""
    def generate(p_rng: np.random.Generator, p_count: int) -> np.ndarray:
        values = p_rng.integers(0, 16, (p_count, 9)).astype(np.float32).view(np.uint32)
        return np.concatenate([values, generate_breaks(p_rng, p_count, p_break_probability)[:, None]], axis=1)

    def expected(p_words: np.ndarray) -> np.ndarray:
        values = np.ascontiguousarray(p_words[:, :9]).view(np.float32).astype(np.int64)
        sums, breaks = segmented_sums(values, p_words[:, 9])
        return np.concatenate([sums.astype(np.float32).view(np.uint32), breaks[:, None]], axis=1)

    return ScanElement("SegmentedWrapper<float3x3>", "SegmentedPrefix<float3x3, Add<float3x3>>", 10, generate,
        expected, None, 9)


UINT_ADD = ScanElement("uint", "Add<uint>", 1,
    lambda p_rng, p_count: p_rng.integers(0, 1 << 8, (p_count, 1), dtype=np.uint32), add_expected, 1)
UINT2_ADD = ScanElement("uint2", "Add<uint2>", 2,
    lambda p_rng, p_count: p_rng.integers(0, 2 ** 32, (p_count, 2), dtype=np.uint32), add_expected, 2)


@pytest.fixture
def rng() -> np.random.Generator:
    return np.random.default_rng(RANDOM_SEED)


def create_buffer(p_device: spy.Device, p_values: np.ndarray) -> spy.Buffer:
    return p_device.create_buffer(element_count=len(p_values), struct_size=4,
        usage=spy.BufferUsage.shader_resource | spy.BufferUsage.unordered_access, data=p_values)


def make_scan_data(p_values: np.ndarray) -> np.ndarray:
    """The scanned buffer layout: the GPU vector header with the byte size, the elements, then the guards. p_values
    holds one uint per element, or an (element count, words) array of the words of each element."""
    words = p_values if p_values.ndim == 2 else p_values[:, None]
    data = np.full(DATA_START + words.size + GUARD_COUNT, GUARD_VALUE, dtype=np.uint32)
    data[:DATA_START] = 0
    data[BYTE_SIZE_INDEX] = words.size * WORD_BYTE_SIZE
    data[DATA_START:DATA_START + words.size] = words.ravel()
    return data


def run_large_prefix_scan(p_device: spy.Device, p_load_kernel: KernelLoader, p_data: spy.Buffer,
        p_element_count: int, p_element: ScanElement = UINT_ADD) -> spy.Buffer:
    """Runs the four passes on p_data in place, in order, and returns the sections buffer."""
    block_num = (p_element_count + BLOCK_SIZE - 1) // BLOCK_SIZE
    # Stale totals from a previous scan must not leak into the result.
    sections = create_buffer(p_device, np.full((SECTION_NUM + max(block_num, 1)) * MAX_SECTION_ELEMENT_WORD_COUNT,
        GUARD_VALUE, dtype=np.uint32))
    group_counts = [block_num, SECTION_NUM, 1, block_num]
    for kernel_pass, group_count in enumerate(group_counts):
        if group_count == 0:
            continue
        p_load_kernel("large_prefix_scan.slang", {"KERNEL_PASS": kernel_pass, "ELEMENT_TYPE": p_element.element_type,
            "OPERATOR": p_element.operator}).dispatch(thread_count=[group_count * WORKGROUP_SIZE, 1, 1],
            vars={"data": p_data, "sections": sections})
    return sections


def check_large_prefix_scan(p_device: spy.Device, p_load_kernel: KernelLoader, p_words: np.ndarray,
        p_element: ScanElement = UINT_ADD) -> None:
    """Scans the elements p_words, an (element count, p_element.word_count) array of words, and checks the result, that
    the guards are untouched, and that the last section total is the scan of every element."""
    element_count = len(p_words)
    data = make_scan_data(p_words)
    data_buffer = create_buffer(p_device, data)
    sections = run_large_prefix_scan(p_device, p_load_kernel, data_buffer, element_count, p_element)
    result = data_buffer.to_numpy().view(np.uint32).copy()
    element_words = slice(DATA_START, DATA_START + element_count * p_element.word_count)
    result[element_words] = p_element.canonical(result[element_words].reshape(element_count, p_element.word_count)).ravel()
    expected = data.copy()
    scanned = p_element.expected(p_words)
    expected[element_words] = scanned.ravel()
    np.testing.assert_array_equal(result, expected)
    if element_count == 0 or p_element.section_word_count is None:
        return
    section_words = sections.to_numpy().view(np.uint32)
    last_section = SECTION_TOTALS_START + SECTION_NUM - 1
    last_section_total = section_words[last_section * p_element.section_word_count:
        (last_section + 1) * p_element.section_word_count]
    np.testing.assert_array_equal(p_element.canonical(last_section_total[None, :])[0], scanned[-1])


# Element counts around the block size, with one block per section, and past SECTION_NUM blocks, with several blocks
# per section and a partial last section.
ELEMENT_COUNTS = [1, 3, 255, 256, 257, 1000, BLOCK_SIZE * SECTION_NUM, BLOCK_SIZE * SECTION_NUM + 1,
    BLOCK_SIZE * SECTION_NUM * 2 + 777]
GENERIC_ELEMENT_COUNTS = [0, 1, 257, 1000, BLOCK_SIZE * SECTION_NUM + 1, BLOCK_SIZE * SECTION_NUM * 2 + 777]
# Segmented instantiations: without breaks, the scan runs across every block and section; with rare breaks, segments
# span blocks and sections; with frequent breaks, most segments end within a block.
GENERIC_ELEMENTS = {
    "uint2_add": UINT2_ADD,
    "segmented_uint_no_breaks": segmented_uint_element(0.0),
    "segmented_uint_rare_breaks": segmented_uint_element(0.0005),
    "segmented_uint_frequent_breaks": segmented_uint_element(0.3),
    "segmented_float3x3_no_breaks": segmented_float3x3_element(0.0),
    "segmented_float3x3_rare_breaks": segmented_float3x3_element(0.0005),
    "segmented_float3x3_frequent_breaks": segmented_float3x3_element(0.3),
}


@pytest.mark.parametrize("p_element_count", ELEMENT_COUNTS)
def test_large_prefix_scan(device: spy.Device, load_kernel: KernelLoader, rng: np.random.Generator,
        p_element_count: int) -> None:
    check_large_prefix_scan(device, load_kernel, UINT_ADD.generate(rng, p_element_count))


def test_large_prefix_scan_ones(device: spy.Device, load_kernel: KernelLoader) -> None:
    element_count = BLOCK_SIZE * SECTION_NUM * 2 + 777
    check_large_prefix_scan(device, load_kernel, np.ones((element_count, 1), dtype=np.uint32))


def test_large_prefix_scan_wraps(device: spy.Device, load_kernel: KernelLoader, rng: np.random.Generator) -> None:
    # Sums wrap modulo 2^32 like the reference.
    check_large_prefix_scan(device, load_kernel, rng.integers(0, 2 ** 32, (5000, 1), dtype=np.uint32))


def test_large_prefix_scan_empty(device: spy.Device, load_kernel: KernelLoader) -> None:
    check_large_prefix_scan(device, load_kernel, np.zeros((0, 1), dtype=np.uint32))


@pytest.mark.parametrize("p_element_count", GENERIC_ELEMENT_COUNTS)
@pytest.mark.parametrize("p_element_name", list(GENERIC_ELEMENTS))
def test_large_prefix_scan_generic(device: spy.Device, load_kernel: KernelLoader, rng: np.random.Generator,
        p_element_name: str, p_element_count: int) -> None:
    element = GENERIC_ELEMENTS[p_element_name]
    check_large_prefix_scan(device, load_kernel, element.generate(rng, p_element_count), element)


@pytest.mark.parametrize("p_element", [segmented_uint_element(0.0), segmented_float3x3_element(0.0)],
    ids=["segmented_uint", "segmented_float3x3"])
def test_large_prefix_scan_segmented_boundary_breaks(device: spy.Device, load_kernel: KernelLoader,
        rng: np.random.Generator, p_element: ScanElement) -> None:
    # Segments starting exactly at block starts, one element before or after them, and at section starts, where the
    # block and section offsets must not leak into the next segment.
    element_count = BLOCK_SIZE * SECTION_NUM * 2 + 777
    words = p_element.generate(rng, element_count)
    # The block count is 513, so each section owns ceil(513 / SECTION_NUM) = 3 blocks.
    blocks_per_section = 3
    break_positions = np.concatenate([np.arange(0, element_count, BLOCK_SIZE * 7),
        np.arange(BLOCK_SIZE * 3 - 1, element_count, BLOCK_SIZE * 11),
        np.arange(BLOCK_SIZE * 5 + 1, element_count, BLOCK_SIZE * 13),
        np.arange(0, element_count, BLOCK_SIZE * blocks_per_section * 5)])
    words[break_positions, -1] = 1
    check_large_prefix_scan(device, load_kernel, words, p_element)
