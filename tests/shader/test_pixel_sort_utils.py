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
import colorsys
from pathlib import Path
from typing import Any

import numpy as np
import pytest
import slangpy as spy


RANDOM_SEED = 20251003
SAMPLE_COUNT = 1024
METRIC_LUMINANCE = 0
METRIC_HUE = 1
METRIC_SATURATION = 2
UINT32_MAX = 0xFFFFFFFF


@pytest.fixture(scope="module")
def pixel_sort(device: spy.Device, shader_include_dir: Path) -> spy.Module:
    return spy.Module.load_from_file(device, str(shader_include_dir / "pixel_sort" / "pixel_sort_utils.slang"))


@pytest.fixture
def rng() -> np.random.Generator:
    return np.random.default_rng(RANDOM_SEED)


def call(p_function: Any, *p_args: Any) -> np.ndarray:
    return p_function(*p_args, _result="numpy")


def random_colors(p_rng: np.random.Generator, p_count: int) -> np.ndarray:
    return p_rng.uniform(0.0, 1.0, (p_count, 3)).astype(np.float32)


def key_levels_reference(p_width: int, p_height: int) -> int:
    return UINT32_MAX // (p_width * p_height)


def test_luminance(pixel_sort: spy.Module, rng: np.random.Generator) -> None:
    colors = random_colors(rng, SAMPLE_COUNT)
    expected = np.clip(colors.astype(np.float64) @ np.array([0.2126, 0.7152, 0.0722]), 0.0, 1.0)
    np.testing.assert_allclose(call(pixel_sort.pixel_sort_luminance, colors), expected, atol=1e-6)


def test_luminance_is_clamped(pixel_sort: spy.Module) -> None:
    colors = np.array([[4.0, 4.0, 4.0], [-1.0, -1.0, -1.0]], dtype=np.float32)
    np.testing.assert_allclose(call(pixel_sort.pixel_sort_luminance, colors), [1.0, 0.0])


def test_hue_matches_hsv(pixel_sort: spy.Module, rng: np.random.Generator) -> None:
    colors = random_colors(rng, SAMPLE_COUNT)
    expected = np.array([colorsys.rgb_to_hsv(*color)[0] for color in colors.astype(np.float64)])
    result = call(pixel_sort.pixel_sort_hue, colors)
    # Hue wraps at red, so compare the distance on the unit circle.
    distance = np.abs(result - expected)
    np.testing.assert_allclose(np.minimum(distance, 1.0 - distance), 0.0, atol=1e-5)
    assert np.all((result >= 0.0) & (result < 1.0))


def test_hue_of_primaries_and_gray(pixel_sort: spy.Module) -> None:
    colors = np.array([[1.0, 0.0, 0.0], [1.0, 1.0, 0.0], [0.0, 1.0, 0.0], [0.0, 1.0, 1.0], [0.0, 0.0, 1.0],
        [1.0, 0.0, 1.0], [0.5, 0.5, 0.5], [0.0, 0.0, 0.0]], dtype=np.float32)
    expected = [0.0, 1.0 / 6.0, 2.0 / 6.0, 3.0 / 6.0, 4.0 / 6.0, 5.0 / 6.0, 0.0, 0.0]
    np.testing.assert_allclose(call(pixel_sort.pixel_sort_hue, colors), expected, atol=1e-6)


def test_saturation_matches_hsv(pixel_sort: spy.Module, rng: np.random.Generator) -> None:
    colors = random_colors(rng, SAMPLE_COUNT)
    expected = np.array([colorsys.rgb_to_hsv(*color)[1] for color in colors.astype(np.float64)])
    np.testing.assert_allclose(call(pixel_sort.pixel_sort_saturation, colors), expected, atol=1e-6)


def test_saturation_of_black_is_zero(pixel_sort: spy.Module) -> None:
    colors = np.array([[0.0, 0.0, 0.0], [0.25, 0.25, 0.25]], dtype=np.float32)
    np.testing.assert_allclose(call(pixel_sort.pixel_sort_saturation, colors), [0.0, 0.0])


def test_metric_selects_function(pixel_sort: spy.Module, rng: np.random.Generator) -> None:
    colors = random_colors(rng, SAMPLE_COUNT)
    for metric, function in [(METRIC_LUMINANCE, pixel_sort.pixel_sort_luminance), (METRIC_HUE, pixel_sort.pixel_sort_hue),
            (METRIC_SATURATION, pixel_sort.pixel_sort_saturation)]:
        result = call(pixel_sort.pixel_sort_metric, colors, np.full(SAMPLE_COUNT, metric, dtype=np.uint32))
        np.testing.assert_array_equal(result, call(function, colors), err_msg=f"metric {metric}")


def test_in_contrast_uses_inclusive_range(pixel_sort: spy.Module) -> None:
    # Gray levels have a luminance equal to their channel value.
    colors = np.array([[0.2, 0.2, 0.2], [0.25, 0.25, 0.25], [0.5, 0.5, 0.5], [0.75, 0.75, 0.75], [0.8, 0.8, 0.8]],
        dtype=np.float32)
    contrast = {"min_metric": 0.25, "max_metric": 0.75, "sort_metric": METRIC_LUMINANCE, "sort_scope": 0}
    result = [bool(pixel_sort.pixel_sort_in_contrast(spy.float3(*color), contrast)) for color in colors]
    assert result == [False, True, True, True, False]


def test_in_contrast_follows_sort_metric(pixel_sort: spy.Module) -> None:
    # Pure blue is dark but fully saturated.
    blue = spy.float3(0.0, 0.0, 1.0)
    bright_range = {"min_metric": 0.5, "max_metric": 1.0, "sort_scope": 0}
    assert not pixel_sort.pixel_sort_in_contrast(blue, {**bright_range, "sort_metric": METRIC_LUMINANCE})
    assert pixel_sort.pixel_sort_in_contrast(blue, {**bright_range, "sort_metric": METRIC_SATURATION})


@pytest.mark.parametrize("p_extent", [(1, 1), (16, 12), (640, 480), (1920, 1080)])
def test_key_levels(pixel_sort: spy.Module, p_extent: tuple[int, int]) -> None:
    assert pixel_sort.pixel_sort_key_levels(spy.uint2(*p_extent)) == key_levels_reference(*p_extent)


def test_quantized_metric_stays_below_levels(pixel_sort: spy.Module, rng: np.random.Generator) -> None:
    levels = 7
    colors = np.concatenate([random_colors(rng, SAMPLE_COUNT), np.ones((1, 3), dtype=np.float32)])
    metrics = np.full(len(colors), METRIC_LUMINANCE, dtype=np.uint32)
    result = call(pixel_sort.pixel_sort_quantized_metric, colors, metrics, np.full(len(colors), levels, dtype=np.uint32))
    luminance = call(pixel_sort.pixel_sort_luminance, colors).astype(np.float32)
    np.testing.assert_array_equal(result, np.minimum(np.floor(luminance * np.float32(levels)), levels - 1))
    # A white pixel's metric of 1 maps to the last level instead of overflowing.
    assert result[-1] == levels - 1


def test_line_coordinates_are_vertical_by_default(pixel_sort: spy.Module) -> None:
    assert tuple(pixel_sort.pixel_sort_line_coordinates(spy.uint2(3, 5))) == (3, 5)
    assert tuple(pixel_sort.pixel_sort_line_pixel(spy.uint2(3, 5))) == (3, 5)


def test_make_key_layout_and_round_trip(pixel_sort: spy.Module, rng: np.random.Generator) -> None:
    width, height = 16, 12
    extent = spy.uint2(width, height)
    levels = key_levels_reference(width, height)
    for _ in range(64):
        color = rng.uniform(0.0, 1.0, 3).astype(np.float32)
        x, y = int(rng.integers(0, width)), int(rng.integers(0, height))
        key = int(pixel_sort.pixel_sort_make_key(spy.float3(*color), METRIC_LUMINANCE, spy.uint2(x, y), extent))
        quantized = int(pixel_sort.pixel_sort_quantized_metric(spy.float3(*color), METRIC_LUMINANCE, levels))
        # Columns are the lines by default: the line is x, and the position along it is y.
        assert key == (x * levels + quantized) * height + y
        assert tuple(pixel_sort.pixel_sort_key_pixel(key, x, extent)) == (x, y)


def test_keys_group_by_line_then_metric(pixel_sort: spy.Module) -> None:
    width, height = 8, 8
    extent = spy.uint2(width, height)

    def key(p_gray: float, p_x: int, p_y: int) -> int:
        return int(pixel_sort.pixel_sort_make_key(spy.float3(p_gray, p_gray, p_gray), METRIC_LUMINANCE,
            spy.uint2(p_x, p_y), extent))

    # A brighter pixel later in the same line sorts after a darker one, regardless of position.
    assert key(0.2, 0, 7) < key(0.8, 0, 0)
    # Every key of a line sorts before every key of the next line.
    assert key(1.0, 0, 7) < key(0.0, 1, 0)
    # The largest key of the image fits in 32 bits.
    assert key(1.0, width - 1, height - 1) <= UINT32_MAX


def test_span_key_layout_and_round_trip(pixel_sort: spy.Module, rng: np.random.Generator) -> None:
    width, height = 16, 12
    extent = spy.uint2(width, height)
    levels = key_levels_reference(width, height)
    span_start, span_length = 37, 5
    for offset in range(span_length):
        color = rng.uniform(0.0, 1.0, 3).astype(np.float32)
        key = int(pixel_sort.pixel_sort_make_span_key(spy.float3(*color), METRIC_LUMINANCE, span_start, span_length,
            offset, extent))
        quantized = int(pixel_sort.pixel_sort_quantized_metric(spy.float3(*color), METRIC_LUMINANCE, levels))
        assert key == span_start * levels + quantized * span_length + offset
        # The key stays inside the span's range.
        assert span_start * levels <= key < (span_start + span_length) * levels
        assert pixel_sort.pixel_sort_span_key_offset(key, span_start, span_length, extent) == offset
