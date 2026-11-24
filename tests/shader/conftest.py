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

import pytest
import slangpy as spy
from wbe_utils_shader_compiler import WBEUtilsShaderCompiler

import build_config


SHADER_INCLUDE_DIR = Path(build_config.root_dir) / build_config.resource_dir / "assets" / "shaders"
# The same include paths the ACP shader compiler passes to slangc.
SHADER_INCLUDE_PATHS = [SHADER_INCLUDE_DIR, SHADER_INCLUDE_DIR / "utils"]
KERNEL_DIR = Path(__file__).parent / "kernels"


@pytest.fixture(scope="session")
def shader_include_dir() -> Path:
    """Shader source root, resolved from the resource directory and used as the Slang include path."""
    return SHADER_INCLUDE_DIR


@pytest.fixture(scope="session")
def device(shader_include_dir: Path) -> spy.Device:
    """Vulkan device shared by all shader tests, with the engine shader include paths."""
    return spy.create_device(type=spy.DeviceType.vulkan, include_paths=SHADER_INCLUDE_PATHS)


@pytest.fixture(scope="session")
def load_kernel(device: spy.Device) -> Callable[[str, dict[str, str | int]], spy.ComputeKernel]:
    """
    Loader for the test kernels under tests/shader/kernels.

    Each call compiles the kernel's `main` entry point in a new Slang session whose preprocessor defines are the given
    ones, so kernels can be specialized per test.
    """
    def load(p_kernel_name: str, p_defines: dict[str, str | int]) -> spy.ComputeKernel:
        session = device.create_slang_session(compiler_options={"include_paths": [*SHADER_INCLUDE_PATHS, KERNEL_DIR],
            "defines": {name: str(value) for name, value in p_defines.items()}})
        module = session.load_module(p_kernel_name)
        return device.create_compute_kernel(session.link_program([module], [module.entry_point("main")]))
    return load


KernelFunctionLoader = Callable[[str, str, dict[str, str | int]], spy.ComputeKernel]


@pytest.fixture(scope="session")
def load_kernel_function(device: spy.Device, tmp_path_factory: pytest.TempPathFactory) -> KernelFunctionLoader:
    """
    Loader for the [wbe::kernel] functions of the engine shaders, through their generated test entry points.

    Each call reflects the kernel in a shader source, given relative to the shader root, with the given preprocessor
    defines, and compiles the test entry point wbe-utils-shader-compiler generates for it. The entry point declares each
    wbe::bind and wbe::push parameter as a global named as the parameter without its `p_` prefix, so a dispatch binds
    them by those names.
    """
    shader_compiler = WBEUtilsShaderCompiler()
    generated_dir = tmp_path_factory.mktemp("kernel_functions")
    generated_count = 0

    def load(p_source: str, p_kernel_name: str, p_defines: dict[str, str | int]) -> spy.ComputeKernel:
        nonlocal generated_count
        defines = {name: str(value) for name, value in p_defines.items()}
        kernel = shader_compiler.reflect_kernel(SHADER_INCLUDE_DIR / p_source, p_kernel_name, SHADER_INCLUDE_PATHS,
            list(defines.items()))
        generated_path = generated_dir / f"{p_kernel_name}_{generated_count}.slang"
        generated_count += 1
        generated_path.write_text(shader_compiler.generate_test_source(kernel), encoding="utf-8")
        session = device.create_slang_session(compiler_options={"include_paths": SHADER_INCLUDE_PATHS,
            "defines": defines})
        module = session.load_module(generated_path.as_posix())
        return device.create_compute_kernel(session.link_program([module], [module.entry_point("main")]))
    return load
