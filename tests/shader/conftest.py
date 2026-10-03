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

import pytest
import slangpy as spy

import build_config


SHADER_INCLUDE_DIR = Path(build_config.root_dir) / build_config.resource_dir / "assets" / "shaders"


@pytest.fixture(scope="session")
def shader_include_dir() -> Path:
    """Shader source root, resolved from the resource directory and used as the Slang include path."""
    return SHADER_INCLUDE_DIR


@pytest.fixture(scope="session")
def device(shader_include_dir: Path) -> spy.Device:
    """Vulkan device shared by all shader tests, with the engine shader root as its include path."""
    return spy.create_device(type=spy.DeviceType.vulkan, include_paths=[shader_include_dir])
