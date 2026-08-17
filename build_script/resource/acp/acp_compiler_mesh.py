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
import re
import struct
from typing import Any, override
from build_script.resource.acp.acp_compiler import ManifestResource, WBEACPCompiler


class WBEACPCompilerMesh(WBEACPCompiler):
    """Compile mesh and submesh source geometry into runtime binary sections."""

    _SOURCE_ONLY_FIELDS = {"vertices_data", "indices_data", "geometry_output_dir", "_geometry_output_name", "source_type", "source_file"}
    _SECTION_TYPES = {
        "position": "vec3",
        "normal": "vec3",
        "tangent": "vec3",
        "bitangent": "vec3",
        "uv": "vec2",
        "index": "uint32",
    }
    _OPTIONAL_VERTEX_SLOTS = ["normal", "tangent", "bitangent", "uv"]

    @override
    def get_supported_resource_types(self) -> list[str]:
        return ["mesh", "submesh"]

    @override
    def compile(self, resource: ManifestResource, manifest_path: Path,
            res_dir: Path, res_output_dir: Path) -> list[ManifestResource]:
        resource_type = resource.get("source_type", resource.get("type"))
        if resource_type == "mesh":
            return [self._compile_mesh(resource, manifest_path)]
        if resource_type == "submesh":
            return [self._compile_submesh(resource, manifest_path, res_output_dir)]
        raise RuntimeError(
            f"WBEACPCompilerMesh does not support resource type '{resource_type}' in {manifest_path}."
        )

    def _compile_mesh(self, resource: ManifestResource, manifest_path: Path) -> ManifestResource:
        if "vertices_data" in resource or "indices_data" in resource:
            raise RuntimeError(
                f"WBEACPCompilerMesh: mesh resources must declare geometry through submeshes in {manifest_path}."
            )

        result = self._copy_runtime_fields(resource)
        result.setdefault("type", "mesh")
        geometry_output_dir = resource.get("geometry_output_dir")
        submeshes = result.get("submeshes")
        if submeshes is None:
            return result
        if not isinstance(submeshes, list):
            raise RuntimeError(
                f"WBEACPCompilerMesh: mesh submeshes must be an array in {manifest_path}."
            )

        resource_id = resource.get("id")
        result_submeshes: list[Any] = []
        for submesh_index, submesh in enumerate(submeshes):
            if not isinstance(submesh, dict):
                result_submeshes.append(submesh)
                continue
            result_submesh = dict(submesh)
            if geometry_output_dir is not None and "geometry_output_dir" not in result_submesh:
                result_submesh["geometry_output_dir"] = geometry_output_dir
            result_submesh.setdefault("_geometry_output_name", self._nested_submesh_output_name(resource_id, result_submesh, submesh_index, manifest_path))
            result_submeshes.append(result_submesh)
        result["submeshes"] = result_submeshes
        return result

    def _compile_submesh(self, resource: ManifestResource, manifest_path: Path, res_output_dir: Path) -> ManifestResource:
        has_inline_vertices = "vertices_data" in resource
        has_inline_indices = "indices_data" in resource
        has_runtime_geometry = "geometry_path" in resource or "geometry_sections" in resource
        if has_runtime_geometry and (has_inline_vertices or has_inline_indices):
            raise RuntimeError(
                f"WBEACPCompilerMesh: submesh cannot mix inline geometry and geometry sections in {manifest_path}."
            )
        if has_runtime_geometry:
            self._validate_runtime_submesh(resource, manifest_path)
            result = self._copy_runtime_fields(resource)
            result.setdefault("type", "submesh")
            return result
        if has_inline_vertices != has_inline_indices:
            raise RuntimeError(
                f"WBEACPCompilerMesh: inline submesh requires both vertices_data and indices_data in {manifest_path}."
            )
        if not has_inline_vertices:
            raise RuntimeError(
                f"WBEACPCompilerMesh: submesh requires geometry_path/geometry_sections or vertices_data/indices_data in {manifest_path}."
            )

        vertices = resource["vertices_data"]
        indices = resource["indices_data"]
        if not isinstance(vertices, list) or not isinstance(indices, list):
            raise RuntimeError(
                f"WBEACPCompilerMesh: vertices_data and indices_data must be arrays in {manifest_path}."
            )

        output_rel_path = self._geometry_output_path(resource, manifest_path)
        output_path = Path.joinpath(res_output_dir, output_rel_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        payload, sections = self._build_geometry_payload(vertices, indices, manifest_path)
        output_path.write_bytes(payload)

        result = self._copy_runtime_fields(resource)
        result.setdefault("type", "submesh")
        result["geometry_path"] = output_rel_path.as_posix()
        result["geometry_sections"] = sections
        return result

    def _copy_runtime_fields(self, resource: ManifestResource) -> ManifestResource:
        return {key: value for key, value in resource.items() if key not in self._SOURCE_ONLY_FIELDS}

    def _nested_submesh_output_name(self, resource_id: Any, submesh: ManifestResource, submesh_index: int, manifest_path: Path) -> str:
        submesh_id = submesh.get("id")
        if isinstance(submesh_id, str) and submesh_id:
            return submesh_id
        if isinstance(resource_id, str) and resource_id:
            return f"{resource_id}.submesh.{submesh_index}"
        return f"{manifest_path.stem}.submesh.{submesh_index}"

    def _geometry_output_path(self, resource: ManifestResource, manifest_path: Path) -> Path:
        raw_output_dir = resource.get("geometry_output_dir", manifest_path.parent.as_posix())
        if not isinstance(raw_output_dir, str):
            raise RuntimeError(
                f"WBEACPCompilerMesh: geometry_output_dir must be a string in {manifest_path}."
            )
        output_dir = Path(raw_output_dir)
        if output_dir.is_absolute():
            raise RuntimeError(
                f"WBEACPCompilerMesh: geometry_output_dir must be resource-root-relative in {manifest_path}: {raw_output_dir}."
            )
        output_dir = Path(output_dir.as_posix())
        output_name = resource.get("_geometry_output_name", resource.get("id", manifest_path.stem))
        if not isinstance(output_name, str) or not output_name:
            output_name = manifest_path.stem
        return output_dir / f"{self._sanitize_file_stem(output_name)}.geometry.bin"

    def _sanitize_file_stem(self, value: str) -> str:
        sanitized = re.sub(r"[^A-Za-z0-9_.-]+", "_", value.strip())
        return sanitized if sanitized else "submesh"

    def _build_geometry_payload(self, vertices: list[Any], indices: list[Any], manifest_path: Path) -> tuple[bytes, list[dict[str, Any]]]:
        payload = bytearray()
        sections: list[dict[str, Any]] = []
        self._append_float_section(payload, sections, "position", vertices, manifest_path, True)
        for slot in self._OPTIONAL_VERTEX_SLOTS:
            if self._has_vertex_slot(vertices, slot):
                self._append_float_section(payload, sections, slot, vertices, manifest_path, False)
        self._append_index_section(payload, sections, indices, len(vertices), manifest_path)
        return bytes(payload), sections

    def _append_float_section(self, payload: bytearray, sections: list[dict[str, Any]], slot: str,
            vertices: list[Any], manifest_path: Path, is_required: bool) -> None:
        start = len(payload)
        for vertex in vertices:
            if not isinstance(vertex, dict):
                raise RuntimeError(
                    f"WBEACPCompilerMesh: vertices_data entries must be objects in {manifest_path}."
                )
            components = 2 if slot == "uv" else 3
            values = self._read_vertex_vector(vertex, slot, components, manifest_path, is_required)
            payload.extend(struct.pack("<" + "f" * components, *values))
        sections.append({
            "slot": slot,
            "start": start,
            "size": len(payload) - start,
            "type": self._SECTION_TYPES[slot],
        })

    def _append_index_section(self, payload: bytearray, sections: list[dict[str, Any]],
            indices: list[Any], vertex_count: int, manifest_path: Path) -> None:
        if len(indices) % 3 != 0:
            raise RuntimeError(
                f"WBEACPCompilerMesh: indices_data size {len(indices)} is not a multiple of 3 in {manifest_path}."
            )
        start = len(payload)
        for index in indices:
            if not isinstance(index, int) or isinstance(index, bool):
                raise RuntimeError(
                    f"WBEACPCompilerMesh: indices_data entries must be integers in {manifest_path}."
                )
            if index < 0 or index >= vertex_count:
                raise RuntimeError(
                    f"WBEACPCompilerMesh: index {index} is out of range for {vertex_count} vertices in {manifest_path}."
                )
            payload.extend(struct.pack("<I", index))
        sections.append({
            "slot": "index",
            "start": start,
            "size": len(payload) - start,
            "type": "uint32",
        })

    def _has_vertex_slot(self, vertices: list[Any], slot: str) -> bool:
        return any(isinstance(vertex, dict) and vertex.get(slot) is not None for vertex in vertices)

    def _read_vertex_vector(self, vertex: ManifestResource, slot: str, component_count: int,
            manifest_path: Path, is_required: bool) -> tuple[float, ...]:
        raw_value = vertex.get(slot)
        if raw_value is None:
            if is_required:
                raise RuntimeError(
                    f"WBEACPCompilerMesh: vertex is missing required '{slot}' in {manifest_path}."
                )
            return tuple(0.0 for _ in range(component_count))
        if not isinstance(raw_value, dict):
            raise RuntimeError(
                f"WBEACPCompilerMesh: vertex '{slot}' must be an object in {manifest_path}."
            )
        component_names = ("u", "v") if slot == "uv" else ("x", "y", "z")
        fallback_component_names = ("x", "y") if slot == "uv" else component_names
        values: list[float] = []
        for component_name, fallback_component_name in zip(component_names, fallback_component_names):
            component = raw_value.get(component_name, raw_value.get(fallback_component_name))
            if not isinstance(component, int | float) or isinstance(component, bool):
                raise RuntimeError(
                    f"WBEACPCompilerMesh: vertex '{slot}.{component_name}' must be numeric in {manifest_path}."
                )
            values.append(float(component))
        return tuple(values)

    def _validate_runtime_submesh(self, resource: ManifestResource, manifest_path: Path) -> None:
        geometry_path = resource.get("geometry_path")
        geometry_sections = resource.get("geometry_sections")
        if not isinstance(geometry_path, str) or not geometry_path:
            raise RuntimeError(
                f"WBEACPCompilerMesh: submesh geometry_path must be a non-empty string in {manifest_path}."
            )
        if not isinstance(geometry_sections, list):
            raise RuntimeError(
                f"WBEACPCompilerMesh: submesh geometry_sections must be an array in {manifest_path}."
            )
        seen_slots: set[str] = set()
        for section in geometry_sections:
            if not isinstance(section, dict):
                raise RuntimeError(
                    f"WBEACPCompilerMesh: geometry section entries must be objects in {manifest_path}."
                )
            slot = section.get("slot")
            if not isinstance(slot, str) or slot not in self._SECTION_TYPES:
                raise RuntimeError(
                    f"WBEACPCompilerMesh: unsupported geometry section slot in {manifest_path}: {slot}."
                )
            if slot in seen_slots:
                raise RuntimeError(
                    f"WBEACPCompilerMesh: duplicate geometry section slot '{slot}' in {manifest_path}."
                )
            seen_slots.add(slot)
            if section.get("type") != self._SECTION_TYPES[slot]:
                raise RuntimeError(
                    f"WBEACPCompilerMesh: geometry slot '{slot}' has invalid type in {manifest_path}."
                )
            for key in ("start", "size"):
                value = section.get(key)
                if not isinstance(value, int) or isinstance(value, bool) or value < 0:
                    raise RuntimeError(
                        f"WBEACPCompilerMesh: geometry section '{slot}' {key} must be a non-negative integer in {manifest_path}."
                    )
        for required_slot in ("position", "index"):
            if required_slot not in seen_slots:
                raise RuntimeError(
                    f"WBEACPCompilerMesh: missing required geometry section '{required_slot}' in {manifest_path}."
                )