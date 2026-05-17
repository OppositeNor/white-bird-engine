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
from build_script.resource.acp.acp_compiler import ManifestResource, WBEACPCompiler
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
                res_dir: Path, res_output_dir: Path) -> ManifestResource:
        result = dict(resource)
        metadata_rel_path = result.get("metadata")
        if not isinstance(metadata_rel_path, str) or not metadata_rel_path:
            raise RuntimeError(
                f"Shader resource missing metadata path in {manifest_path}: {resource}."
            )

        metadata_rel = self._normalize_rel_resource_path(metadata_rel_path, manifest_path)
        source_rel_path = result.get("src")
        # Preferred schema: compile source shader and emit normalized runtime fields.
        if isinstance(source_rel_path, str) and source_rel_path:
            destination_rel_path = result.get("dst")
            if not isinstance(destination_rel_path, str) or not destination_rel_path:
                raise RuntimeError(
                    f"Shader resource with src must also declare dst in {manifest_path}: {resource}."
                )
            src_rel = self._normalize_rel_resource_path(source_rel_path, manifest_path)
            dst_rel = self._normalize_rel_resource_path(destination_rel_path, manifest_path)
            self._compile_resource(src_rel, dst_rel, metadata_rel, res_dir, res_output_dir)
            result["path"] = dst_rel.as_posix()
            result["metadata"] = metadata_rel.as_posix()
            if "src" in result:
                del result["src"]
            if "dst" in result:
                del result["dst"]
            return result

        raise RuntimeError(
            f"Shader resource must provide src/dst fields in {manifest_path}: {resource}."
        )

    def _normalize_rel_resource_path(self, resource_path: str, manifest_path: Path) -> Path:
        # Keep resource references relative so they remain portable across build machines.
        relative_path = Path(resource_path)
        if relative_path.is_absolute():
            raise RuntimeError(
                f"Absolute resource path is not supported in {manifest_path}: {resource_path}."
            )
        return relative_path

    def _compile_resource(self, source_rel_path: Path, destination_rel_path: Path,
                          metadata_rel_path: Path, res_dir: Path, res_output_dir: Path) -> None:
        source_file_path = Path.joinpath(res_dir, source_rel_path)
        output_file_path = Path.joinpath(res_output_dir, destination_rel_path)
        shader_metadata_file_path = Path.joinpath(res_output_dir, metadata_rel_path)

        if not source_file_path.exists() or not source_file_path.is_file():
            raise RuntimeError(f"Shader source file not found: {source_rel_path.as_posix()}.")

        os.makedirs(output_file_path.parent, exist_ok=True)
        os.makedirs(shader_metadata_file_path.parent, exist_ok=True)
        if source_file_path.suffix == ".slang":
            self._compile_slang(source_file_path, output_file_path, shader_metadata_file_path)
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

    def _compile_slang(self, shader_file: Path, output_file: Path, shader_metadata_file_path: Path) -> None:
        shader_stage = self._read_shader_stage(shader_file)

        slangc_path = shutil.which("slangc")
        if slangc_path is not None:
            self._compile_slangc_slang(slangc_path, shader_file, output_file, shader_metadata_file_path, shader_stage)
            return
        raise RuntimeError("Failed to find valid slang compiler.")

    def _compile_slangc_slang(self, slangc_path: str, shader_file: Path,
                              output_file: Path, shader_metadata_file: Path, shader_stage: str) -> None:
        shader_stage = self._read_shader_stage(shader_file)
        cmd = [
            slangc_path,
            "-target", "spirv",
            "-entry", "main",
            "-stage", shader_stage,
            "-I", str(self._shader_include_dir),
            "-I", str(self._shader_include_dir / "utils"),
            "-o", str(output_file),
            "-reflection-json", str(shader_metadata_file),
            str(shader_file),
        ]
        if build_setup.build_target["generate-tests"]:
            cmd.extend(["-g3"])
        print("Compiling:", shader_file)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Failed to compile", shader_file)
            print(result.stderr)
            raise RuntimeError("Failed to compile shader.")
        self._normalize_shader_metadata(shader_metadata_file)

    def _normalize_shader_metadata(self, shader_metadata_file: Path) -> None:
        with open(shader_metadata_file, "r", encoding="utf-8") as metadata_file:
            raw_metadata = json.load(metadata_file)
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

    def _transform_shader_type_atom(self, raw_type: dict[str, Any]) -> dict[str, Any]:
        element_type = raw_type.get("elementType", {})
        if not isinstance(element_type, dict):
            element_type = {}
        result_type = raw_type.get("resultType", {})
        if not isinstance(result_type, dict):
            result_type = {}
        scalar_type = raw_type.get("scalarType")
        if not isinstance(scalar_type, str):
            scalar_type = raw_type.get("baseShape", raw_type.get("kind", ""))
        element_scalar_type = element_type.get("scalarType", result_type.get("scalarType", ""))
        return {
            "kind": str(raw_type.get("kind", "")),
            "scalar_type": str(scalar_type),
            "name": str(raw_type.get("name", "")),
            "element_count": int(raw_type.get("elementCount", 0)),
            "element_scalar_type": str(element_scalar_type),
        }

    def _transform_shader_type_field(self, raw_field: dict[str, Any]) -> dict[str, Any]:
        return {
            "name": str(raw_field.get("name", "")),
            "type": self._transform_shader_type_atom(raw_field.get("type", {})),
            "stage": str(raw_field.get("stage", "")),
            "binding": self._transform_shader_binding(raw_field.get("binding", {})),
            "semantic_name": str(raw_field.get("semanticName", "")),
            "semantic_index": int(raw_field.get("semanticIndex", 0)),
        }

    def _transform_shader_type(self, raw_type: dict[str, Any]) -> dict[str, Any]:
        kind = str(raw_type.get("kind", ""))
        element_type = raw_type.get("elementType", {})
        if not isinstance(element_type, dict):
            element_type = {}
        fields = raw_type.get("fields", [])
        if not isinstance(fields, list):
            fields = []
        if kind == "constantBuffer" and isinstance(element_type, dict):
            fields = element_type.get("fields", []) if isinstance(element_type.get("fields", []), list) else []
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

