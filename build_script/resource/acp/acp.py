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

import json
import os
from pathlib import Path
from typing import Any
from build_script.resource.acp.acp_compiler import WBEACPCompiler

class WBEACP:
    """The asset conditioning pipeline.

    This pipeline consumes manifest files from the source resource directory,
    compiles/copies entries by resource type, and emits runtime index files
    into the output resource directory.
    """

    def __init__(self, res_dir: Path, res_output_dir: Path):
        self._res_dir = res_dir
        self._res_output_dir = res_output_dir
        self._compiler_map: dict[str, WBEACPCompiler] = {}
        self._manifest_sources: dict[str, dict[str, Any]] = {}
        self._processed_manifests: dict[str, dict[str, Any]] = {}
        self._seen_resource_ids: set[str] = set()

    def add_compiler(self, compiler: WBEACPCompiler):
        """Register a compiler for one or more resource types.

        Args:
            compiler: Compiler implementation.
        """
        for resource_type in compiler.get_supported_resource_types():
            if resource_type in self._compiler_map:
                raise RuntimeError(
                    f"WBEACP: resource type '{resource_type}' already has a compiler registered."
                )
            self._compiler_map[resource_type] = compiler

    def process_resources(self):
        """Process manifests and generate runtime index files."""
        self._manifest_sources = self._load_manifest_sources()
        self._processed_manifests.clear()
        self._seen_resource_ids.clear()
        if not self._manifest_sources:
            print(f"WBEACP: No manifest files found under {self._res_dir}.")
            return

        for manifest_rel_path in sorted(self._manifest_sources.keys()):
            self._process_manifest(manifest_rel_path, [])

    def _load_manifest_sources(self) -> dict[str, dict[str, Any]]:
        manifest_sources: dict[str, dict[str, Any]] = {}
        for json_file in sorted(Path(self._res_dir).rglob("*.json")):
            with open(json_file, "r", encoding="utf-8") as file:
                data = json.load(file)
            if not isinstance(data, dict) or "resources" not in data:
                continue
            if not isinstance(data["resources"], list):
                raise RuntimeError(
                    f"WBEACP: Manifest resources must be an array: {json_file}."
                )

            include_entries = data.get("include", [])
            if include_entries is None:
                include_entries = []
            if not isinstance(include_entries, list):
                raise RuntimeError(
                    f"WBEACP: Manifest include field must be an array: {json_file}."
                )

            normalized_include: list[str] = []
            for include_entry in include_entries:
                if not isinstance(include_entry, str) or not include_entry:
                    raise RuntimeError(
                        f"WBEACP: Manifest include entry must be a non-empty string: {json_file}."
                    )
                normalized_include.append(Path(include_entry).as_posix())

            data["include"] = normalized_include
            relative_manifest_path = json_file.relative_to(self._res_dir).as_posix()
            manifest_sources[relative_manifest_path] = data
        return manifest_sources

    def _process_manifest(self, manifest_rel_path: str, include_stack: list[str]):
        if manifest_rel_path in self._processed_manifests:
            return
        if manifest_rel_path in include_stack:
            recursion_chain = include_stack + [manifest_rel_path]
            raise RuntimeError(
                f"WBEACP: Manifest include recursion detected: {' -> '.join(recursion_chain)}."
            )

        source_manifest = self._manifest_sources.get(manifest_rel_path)
        if source_manifest is None:
            raise RuntimeError(
                f"WBEACP: Manifest include target was not found: {manifest_rel_path}."
            )

        include_stack.append(manifest_rel_path)
        include_paths = source_manifest.get("include", [])
        for include_path in include_paths:
            if include_path not in self._manifest_sources:
                raise RuntimeError(
                    f"WBEACP: Manifest include target does not exist: {include_path} (included by {manifest_rel_path})."
                )
            self._process_manifest(include_path, include_stack)

        processed_resources: list[dict[str, Any]] = []
        for resource in source_manifest["resources"]:
            if not isinstance(resource, dict):
                raise RuntimeError(
                    f"WBEACP: Manifest resource entry must be an object in {manifest_rel_path}."
                )
            processed_resources.append(self._process_typed_resource(resource, manifest_rel_path, True))

        processed_manifest: dict[str, Any] = {
            "resources": processed_resources,
        }
        if "include" in source_manifest:
            processed_manifest["include"] = include_paths

        self._write_manifest(manifest_rel_path, processed_manifest)
        self._processed_manifests[manifest_rel_path] = processed_manifest
        include_stack.pop()

    def _write_manifest(self, manifest_rel_path: str, manifest_data: dict[str, Any]):
        output_path = Path.joinpath(self._res_output_dir, Path(manifest_rel_path))
        os.makedirs(output_path.parent, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as output_file:
            json.dump(manifest_data, output_file, indent=4)
            output_file.write("\n")

    def _process_value(self, value: Any, manifest_rel_path: str) -> Any:
        if isinstance(value, list):
            return [self._process_value(element, manifest_rel_path) for element in value]
        if isinstance(value, dict):
            if self._is_typed_resource(value):
                return self._process_typed_resource(value, manifest_rel_path, False)
            result: dict[str, Any] = {}
            for key, nested_value in value.items():
                result[key] = self._process_value(nested_value, manifest_rel_path)
            return result
        return value

    def _is_typed_resource(self, value: dict[str, Any]) -> bool:
        resource_type = value.get("type")
        return isinstance(resource_type, str) and bool(resource_type)

    def _process_typed_resource(self, resource: dict[str, Any],
                                manifest_rel_path: str, is_top_level: bool) -> dict[str, Any]:
        if not self._is_typed_resource(resource):
            raise RuntimeError(
                f"WBEACP: Typed resource is missing required type field in {manifest_rel_path}: {resource}."
            )

        result = dict(resource)
        if is_top_level:
            resource_id = result.get("id")
            if isinstance(resource_id, str) and resource_id:
                if resource_id in self._seen_resource_ids:
                    raise RuntimeError(
                        f"WBEACP: Duplicated resource id '{resource_id}' found in {manifest_rel_path}."
                    )
                self._seen_resource_ids.add(resource_id)

        resource_type = result["type"]
        compiler = self._compiler_map.get(resource_type)
        if compiler is not None:
            result = compiler.compile(result, Path(manifest_rel_path), self._res_dir, self._res_output_dir)
        else:
            result = self._validate_uncompiled_resource(result, manifest_rel_path)

        normalized_result: dict[str, Any] = {}
        for key, value in result.items():
            normalized_result[key] = self._process_value(value, manifest_rel_path)
        return normalized_result

    def _validate_uncompiled_resource(self, resource: dict[str, Any], manifest_rel_path: str) -> dict[str, Any]:
        result = dict(resource)
        resource_type = str(result.get("type"))
        if "file" in result:
            raise RuntimeError(
                f"WBEACP: Resource type '{resource_type}' declares file in {manifest_rel_path} but has no registered compiler."
            )
        return result
