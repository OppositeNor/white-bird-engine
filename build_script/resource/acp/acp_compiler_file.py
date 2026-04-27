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

import os
from pathlib import Path
import shutil
from typing import override
from build_script.resource.acp.acp_compiler import ManifestResource, WBEACPCompiler


class WBEACPCompilerFile(WBEACPCompiler):
    """ACP compiler that copies one file-backed resource into the output tree."""

    def __init__(self, supported_resource_types: list[str]) -> None:
        self._supported_resource_types = supported_resource_types

    @override
    def get_supported_resource_types(self) -> list[str]:
        return self._supported_resource_types

    @override
    def compile(self, resource: ManifestResource, manifest_path: Path,
                res_dir: Path, res_output_dir: Path) -> ManifestResource:
        result = dict(resource)
        resource_file = result.get("file")
        if not isinstance(resource_file, str) or not resource_file:
            raise RuntimeError(
                f"File resource must declare a non-empty file field in {manifest_path}: {resource}."
            )

        relative_file = Path(resource_file)
        if relative_file.is_absolute():
            raise RuntimeError(
                f"Absolute resource path is not supported in {manifest_path}: {resource_file}."
            )

        source_path = Path.joinpath(res_dir, relative_file)
        if not source_path.exists() or not source_path.is_file():
            raise RuntimeError(
                f"WBEACP: File resource does not exist for {manifest_path}: {relative_file.as_posix()}."
            )

        output_path = Path.joinpath(res_output_dir, relative_file)
        os.makedirs(output_path.parent, exist_ok=True)
        shutil.copyfile(source_path, output_path)

        result["path"] = relative_file.as_posix()
        if "file" in result:
            del result["file"]
        return result
