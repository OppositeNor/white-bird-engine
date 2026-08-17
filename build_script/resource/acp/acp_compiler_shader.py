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
import subprocess
import shutil
from typing import Any, override
from build_script.resource.acp.acp_compiler import ManifestResource, SOURCE_RESOURCE_TYPE_KEY, WBEACPCompiler
import build_setup


class WBEACPCompilerShader(WBEACPCompiler):
    """The shader compiler.
    """
    def __init__(self, shader_include_dir: Path) -> None:
        """Constructor.

        Args:
            shader_include_dir: The include directory for the shaders.
        """
        self._shader_include_dir = shader_include_dir

    @override
    def get_supported_resource_types(self) -> list[str]:
        return ["shader"]

    @override
    def compile(self, resource: ManifestResource, manifest_path: Path,
            res_dir: Path, res_output_dir: Path) -> list[ManifestResource]:
        result = dict(resource)
        result.setdefault("type", self._get_source_resource_type(result) or "shader")
        metadata_rel_path = result.get("metadata")
        if not isinstance(metadata_rel_path, str) or not metadata_rel_path:
            raise RuntimeError(
                f"Shader resource missing metadata path in {manifest_path}: {resource}."
            )

        metadata_rel = self._normalize_resource_path(metadata_rel_path, manifest_path)
        source_rel_path = result.get("src")
        # Preferred schema: compile source shader and emit normalized runtime fields.
        if isinstance(source_rel_path, str) and source_rel_path:
            destination_rel_path = result.get("dst")
            if not isinstance(destination_rel_path, str) or not destination_rel_path:
                raise RuntimeError(
                    f"Shader resource with src must also declare dst in {manifest_path}: {resource}."
                )
            stage = result.get("stage")
            if stage is not None and not isinstance(stage, str):
                raise RuntimeError(
                    f"Shader resource stage must be a string in {manifest_path}: {resource}."
                )
            entry = result.get("entry")
            if entry is not None and not isinstance(entry, str):
                raise RuntimeError(
                    f"Shader resource entry must be a string in {manifest_path}: {resource}."
                )
            source_file_path, _ = self._resolve_resource_file_path(source_rel_path, manifest_path, res_dir)
            dst_rel = self._normalize_resource_path(destination_rel_path, manifest_path)
            self._compile_resource(source_file_path, dst_rel, metadata_rel, res_output_dir, stage, entry, result)
            result["path"] = dst_rel.as_posix()
            result["metadata"] = metadata_rel.as_posix()
            result.pop(SOURCE_RESOURCE_TYPE_KEY, None)
            if "src" in result:
                del result["src"]
            if "dst" in result:
                del result["dst"]
            return [result]

        raise RuntimeError(
            f"Shader resource must provide src/dst fields in {manifest_path}: {resource}."
        )

    def _compile_resource(self, source_file_path: Path, destination_rel_path: Path,
                          metadata_rel_path: Path, res_output_dir: Path,
                          stage: str | None, entry: str | None, resource: ManifestResource) -> None:
        output_file_path = Path.joinpath(res_output_dir, destination_rel_path)
        shader_metadata_file_path = Path.joinpath(res_output_dir, metadata_rel_path)

        if not source_file_path.exists() or not source_file_path.is_file():
            raise RuntimeError(f"Shader source file not found: {source_file_path.as_posix()}.")

        os.makedirs(output_file_path.parent, exist_ok=True)
        os.makedirs(shader_metadata_file_path.parent, exist_ok=True)
        if source_file_path.suffix == ".slang":
            self._compile_slang(source_file_path, output_file_path, shader_metadata_file_path, stage, entry, resource)
        else:
            raise RuntimeError(f"Unsupported shader file type: {source_file_path.suffix}.")

    def _read_shader_stage(self, shader_file: Path) -> str:
        with open(shader_file, "r", encoding="utf-8") as f:
            first_line = f.readline().strip()
        # The shader stage is marked at the beginning of the file starting with a "//"
        if first_line.startswith("//"):
            shader_stage = first_line[2:].strip()
            if shader_stage not in ["vertex", "vert", "fragment", "frag", "tesscontrol",
                    "tesc", "tesseval", "tese", "geometry", "geom", "compute", "comp"]:
                raise RuntimeError(f"Unsupported shader stage for {shader_file} : {shader_stage}.")
        else:
            raise RuntimeError(f"File {shader_file} missing a stage header.")
        return shader_stage

    def _compile_slang(self, shader_file: Path, output_file: Path,
                       shader_metadata_file_path: Path, stage: str | None, entry: str | None,
                       resource: ManifestResource) -> None:
        shader_stage = stage
        entry_name = entry

        if shader_stage is None and entry_name is not None:
            raise RuntimeError(
                f"Shader resource for {shader_file.as_posix()} declares an entry but no stage."
            )

        if shader_stage is None and entry_name is None:
            try:
                shader_stage = self._read_shader_stage(shader_file)
                entry_name = "main"
            except RuntimeError:
                shader_stage = None
                entry_name = None
        elif shader_stage is not None and entry_name is None:
            try:
                self._read_shader_stage(shader_file)
                entry_name = "main"
            except RuntimeError:
                shader_stage = None
                entry_name = None

        slangc_path = shutil.which("slangc")
        if slangc_path is not None:
            self._compile_slangc_slang(
                slangc_path,
                shader_file,
                output_file,
                shader_metadata_file_path,
                shader_stage,
                entry_name,
                resource,
            )
            return
        raise RuntimeError("Failed to find valid slang compiler.")

    def _compile_slangc_slang(self, slangc_path: str, shader_file: Path,
                              output_file: Path, shader_metadata_file: Path,
                              shader_stage: str | None, entry_name: str | None,
                              resource: ManifestResource) -> None:
        cmd = [slangc_path, "-target", "spirv"]
        if entry_name is not None:
            cmd.extend(["-entry", entry_name])
        if shader_stage is not None:
            cmd.extend(["-stage", shader_stage])
        cmd.extend([
            "-I", str(self._shader_include_dir),
            "-I", str(self._shader_include_dir / "utils"),
            "-o", str(output_file),
            "-reflection-json", str(shader_metadata_file),
            str(shader_file),
        ])
        if build_setup.build_target["generate-tests"]:
            cmd.extend(["-g3"])
        print("Compiling:", shader_file)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Failed to compile", shader_file)
            print(result.stderr)
            raise RuntimeError("Failed to compile shader.")
        with open(shader_metadata_file, "r", encoding="utf-8") as metadata_file:
            raw_metadata = json.load(metadata_file)
        self._apply_discovered_stage_entries(resource, raw_metadata)
        self._write_normalized_shader_metadata(shader_metadata_file, raw_metadata)

    def _apply_discovered_stage_entries(self, resource: ManifestResource, raw_metadata: dict[str, Any]) -> None:

        stage_entry_keys: dict[str, str] = {
            "vertex": "vertex_entry",
            "vert": "vertex_entry",
            "tesscontrol": "tessellation_entry",
            "tesc": "tessellation_entry",
            "tesseval": "tessellation_entry",
            "tese": "tessellation_entry",
            "geometry": "geometry_entry",
            "geom": "geometry_entry",
            "fragment": "fragment_entry",
            "frag": "fragment_entry",
            "compute": "compute_entry",
            "comp": "compute_entry",
        }
        discovered_entries: dict[str, str] = {}
        for raw_entry_point in raw_metadata.get("entryPoints", []):
            if not isinstance(raw_entry_point, dict):
                continue
            entry_stage = raw_entry_point.get("stage")
            entry_name = raw_entry_point.get("name")
            if not isinstance(entry_stage, str) or not isinstance(entry_name, str):
                continue
            entry_key = stage_entry_keys.get(entry_stage)
            if entry_key is None or entry_key in discovered_entries:
                continue
            discovered_entries[entry_key] = entry_name

        for entry_key, entry_name in discovered_entries.items():
            resource[entry_key] = entry_name

    def _write_normalized_shader_metadata(self, shader_metadata_file: Path, raw_metadata: dict[str, Any]) -> None:
        normalized_metadata = self._transform_shader_metadata(raw_metadata)
        with open(shader_metadata_file, "w", encoding="utf-8") as metadata_file:
            json.dump(normalized_metadata, metadata_file, indent=4)

    def _transform_shader_metadata(self, raw_metadata: dict[str, Any]) -> dict[str, Any]:
        return {
            "parameters": [self._transform_shader_parameter(parameter) for parameter in raw_metadata.get("parameters", [])],
            "entry_points": [self._transform_shader_entry_point(entry_point) for entry_point in raw_metadata.get("entryPoints", [])],
            "bindless_space_index": int(raw_metadata.get("bindlessSpaceIndex", 0)),
        }

    def _transform_shader_binding(self, raw_binding: dict[str, Any]) -> dict[str, Any]:
        return {
            "kind": str(raw_binding.get("kind", "")),
            "index": int(raw_binding.get("index", 0)),
            "count": int(raw_binding.get("count", raw_binding.get("used", 0))),
            "space": int(raw_binding.get("space", 0)),
            "offset": int(raw_binding.get("offset", 0)),
            "size": int(raw_binding.get("size", 0)),
            "element_stride": int(raw_binding.get("elementStride", 0)),
        }

    def _shader_type_shape(self, raw_type: dict[str, Any]) -> str:
        kind = raw_type.get("kind", "")
        if kind == "array":
            element_type = raw_type.get("elementType", {})
            if isinstance(element_type, dict):
                return self._shader_type_shape(element_type)
        scalar_type = raw_type.get("scalarType")
        if isinstance(scalar_type, str):
            return scalar_type
        base_shape = raw_type.get("baseShape")
        if isinstance(base_shape, str):
            return base_shape
        return str(kind)

    def _shader_buffer_size(self, raw_type: dict[str, Any]) -> int:
        if raw_type.get("kind") == "array":
            element_type = raw_type.get("elementType", {})
            if isinstance(element_type, dict):
                return self._shader_buffer_size(element_type)
        element_var_layout = raw_type.get("elementVarLayout", {})
        if isinstance(element_var_layout, dict):
            binding = element_var_layout.get("binding", {})
            if isinstance(binding, dict):
                return int(binding.get("size", 0))
        binding = raw_type.get("binding", {})
        if isinstance(binding, dict):
            return int(binding.get("size", 0))
        return 0

    def _transform_shader_type_atom(self, raw_type: dict[str, Any]) -> dict[str, Any]:
        element_type = raw_type.get("elementType", {})
        if not isinstance(element_type, dict):
            element_type = {}
        result_type = raw_type.get("resultType", {})
        if not isinstance(result_type, dict):
            result_type = {}
        scalar_type = self._shader_type_shape(raw_type)
        element_scalar_type = self._shader_type_shape(element_type) if element_type else self._shader_type_shape(result_type)
        return {
            "kind": str(raw_type.get("kind", "")),
            "scalar_type": str(scalar_type),
            "name": str(raw_type.get("name", "")),
            "element_count": int(raw_type.get("elementCount", 0)),
            "element_scalar_type": str(element_scalar_type),
        }

    def _transform_shader_type_field(self, raw_field: dict[str, Any]) -> dict[str, Any]:
        raw_type = raw_field.get("type", {})
        if not isinstance(raw_type, dict):
            raw_type = {}
        return {
            "name": str(raw_field.get("name", "")),
            "type": self._transform_shader_type_atom(raw_type),
            "stage": str(raw_field.get("stage", "")),
            "binding": self._transform_shader_binding(raw_field.get("binding", {})),
            "buffer_size": self._shader_buffer_size(raw_type),
            "semantic_name": str(raw_field.get("semanticName", "")),
            "semantic_index": int(raw_field.get("semanticIndex", 0)),
        }

    def _flatten_shader_type_fields(self, raw_fields: list[Any]) -> list[dict[str, Any]]:
        result: list[dict[str, Any]] = []
        for raw_field in raw_fields:
            if not isinstance(raw_field, dict):
                continue
            raw_type = raw_field.get("type", {})
            raw_binding = raw_field.get("binding", {})
            if not isinstance(raw_type, dict) or not isinstance(raw_binding, dict):
                result.append(raw_field)
                continue
            element_type = raw_type.get("elementType", {})
            element_fields = element_type.get("fields", []) if isinstance(element_type, dict) else []
            if raw_type.get("kind") != "array" or element_type.get("kind") != "struct" or not isinstance(element_fields, list):
                result.append(raw_field)
                continue
            base_binding_index = int(raw_binding.get("index", 0))
            element_count = int(raw_type.get("elementCount", 0))
            flattened_any = False
            for element_field in element_fields:
                if not isinstance(element_field, dict):
                    continue
                element_binding = element_field.get("binding", {})
                element_field_type = element_field.get("type", {})
                if not isinstance(element_binding, dict) or element_binding.get("kind") != "descriptorTableSlot":
                    continue
                if not isinstance(element_field_type, dict):
                    element_field_type = {}
                flattened_field = dict(element_field)
                flattened_field["name"] = f"{raw_field.get('name', '')}.{element_field.get('name', '')}"
                flattened_field["type"] = {
                    "kind": "array",
                    "elementCount": element_count,
                    "elementType": element_field_type,
                }
                flattened_field["binding"] = {
                    **element_binding,
                    "index": base_binding_index + int(element_binding.get("index", 0)),
                    "count": element_count,
                }
                result.append(flattened_field)
                flattened_any = True
            if not flattened_any:
                result.append(raw_field)
        return result

    def _transform_shader_type(self, raw_type: dict[str, Any]) -> dict[str, Any]:
        kind = str(raw_type.get("kind", ""))
        element_type = raw_type.get("elementType", {})
        if not isinstance(element_type, dict):
            element_type = {}
        fields = raw_type.get("fields", [])
        if not isinstance(fields, list):
            fields = []
        element_var_layout = raw_type.get("elementVarLayout", {})
        element_layout_type = {}
        if isinstance(element_var_layout, dict) and isinstance(element_var_layout.get("type"), dict):
            element_layout_type = element_var_layout["type"]
        if kind in {"constantBuffer", "parameterBlock"}:
            field_source = element_layout_type if element_layout_type else element_type
            fields = field_source.get("fields", []) if isinstance(field_source.get("fields", []), list) else []
            if kind == "parameterBlock":
                fields = self._flatten_shader_type_fields(fields)
        return {
            "kind": kind,
            "name": str(raw_type.get("name", "")),
            "fields": [self._transform_shader_type_field(field) for field in fields if isinstance(field, dict)],
            "element_count": int(raw_type.get("elementCount", 0)),
            "element_type": self._transform_shader_type_atom(element_type),
            "scalar_type": str(raw_type.get("scalarType", raw_type.get("baseShape", ""))),
        }

    def _transform_shader_parameter(self, raw_parameter: dict[str, Any]) -> dict[str, Any]:
        return {
            "name": str(raw_parameter.get("name", "")),
            "stage": str(raw_parameter.get("stage", "")),
            "binding": self._transform_shader_binding(raw_parameter.get("binding", {})),
            "type": self._transform_shader_type(raw_parameter.get("type", {})),
            "semantic_name": str(raw_parameter.get("semanticName", "")),
            "semantic_index": int(raw_parameter.get("semanticIndex", 0)),
        }

    def _transform_shader_result(self, raw_result: dict[str, Any]) -> dict[str, Any]:
        return {
            "stage": str(raw_result.get("stage", "")),
            "binding": self._transform_shader_binding(raw_result.get("binding", {})),
            "semantic_name": str(raw_result.get("semanticName", "")),
            "semantic_index": int(raw_result.get("semanticIndex", 0)),
            "type": self._transform_shader_type(raw_result.get("type", {})),
        }

    def _transform_shader_entry_point(self, raw_entry_point: dict[str, Any]) -> dict[str, Any]:
        bindings = raw_entry_point.get("bindings", [])
        if not isinstance(bindings, list):
            bindings = []
        return {
            "name": str(raw_entry_point.get("name", "")),
            "stage": str(raw_entry_point.get("stage", "")),
            "parameters": [self._transform_shader_parameter(parameter) for parameter in raw_entry_point.get("parameters", []) if isinstance(parameter, dict)],
            "result": self._transform_shader_result(raw_entry_point.get("result", {})),
            "bindings": [self._transform_shader_binding(binding.get("binding", binding)) for binding in bindings if isinstance(binding, dict)],
        }
