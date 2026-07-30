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
from typing import override
from build_script.resource.acp.acp_compiler import ManifestResource, WBEACPCompiler
from wbe_utils_model_compiler import WBEUtilsMeshCompiler

class WBEACPCompilerShader(WBEACPCompiler):
    """The shader compiler.
    """
    def __init__(self) -> None:
        self._utils_mesh_compiler = WBEUtilsMeshCompiler()

    @override
    def get_supported_resource_types(self) -> list[str]:
        return self._utils_mesh_compiler.get_supported_resource_types()

    @override
    def compile(self, resource: ManifestResource, manifest_path: Path,
                res_dir: Path, res_output_dir: Path) -> ManifestResource:
        return {} # TODO
