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

class WBEACPCompilerModel(WBEACPCompiler):
    """The model compiler.
    """
    def __init__(self) -> None:
        self._utils_mesh_compiler = WBEUtilsMeshCompiler()

    @override
    def get_supported_resource_types(self) -> list[str]:
        return self._utils_mesh_compiler.get_supported_resource_types()

    @override
    def compile(self, resource: ManifestResource, manifest_path: Path,
            res_dir: Path, res_output_dir: Path) -> list[ManifestResource]:
        resource_path = resource.get("file")
        if not isinstance(resource_path, str) or not resource_path:
            raise RuntimeError(
                f"Model resource must declare a non-empty 'file' field in {manifest_path}: {resource}."
            )

        resolved_resource = dict(resource)
        source_path, _ = self._resolve_resource_file_path(resource_path, manifest_path, res_dir)
        resolved_resource["path"] = source_path.as_posix()
        resolved_manifest_path = Path.joinpath(res_dir, manifest_path)
        mesh_resource = self._utils_mesh_compiler.compile_mesh(resolved_resource, resolved_manifest_path, res_dir, res_output_dir)
        material_resources = self._utils_mesh_compiler.compile_materials(resolved_resource, resolved_manifest_path, res_dir, res_output_dir)
        return [mesh_resource] + material_resources
