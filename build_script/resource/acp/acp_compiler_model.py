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
import sys
from typing import override
from build_script.resource.acp.acp_compiler import ManifestResource, WBEACPCompiler

_VENDORED_COMPILER_DIR = Path(__file__).resolve().parents[3] / "dependencies" / "wbe-utils-model-compiler"
if _VENDORED_COMPILER_DIR.exists():
    sys.path.insert(0, _VENDORED_COMPILER_DIR.as_posix())

from wbe_utils_model_compiler import WBEUtilsModelCompiler

class WBEACPCompilerModel(WBEACPCompiler):
    """The model compiler.
    """
    def __init__(self, cache_dir: Path | None = None) -> None:
        self._utils_mesh_compiler = WBEUtilsModelCompiler(cache_dir=cache_dir)

    @override
    def get_supported_resource_types(self) -> list[str]:
        return self._utils_mesh_compiler.get_supported_resource_types()

    @override
    def compile(self, resource: ManifestResource, manifest_path: Path,
            res_dir: Path, res_output_dir: Path) -> list[ManifestResource]:
        resource_path = self._get_source_resource_file(resource)
        if resource_path is None:
            raise RuntimeError(
                f"Model resource must declare a non-empty source file field in {manifest_path}: {resource}."
            )

        resolved_resource = dict(resource)
        source_path, _ = self._resolve_resource_file_path(resource_path, manifest_path, res_dir)
        source_type = resource.get("source_type", resource.get("type"))
        resolved_resource["type"] = "static_geometry" if source_type == "static_geometry" else "model"
        resolved_resource["file"] = source_path.as_posix()
        resolved_manifest_path = Path.joinpath(res_dir, manifest_path)
        return self._utils_mesh_compiler.compile(
            resolved_resource,
            resolved_manifest_path,
            res_dir,
            res_output_dir,
        )
