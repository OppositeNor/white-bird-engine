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
from build_script.resource.acp.acp_compiler import ManifestResource, SOURCE_RESOURCE_TYPE_KEY, ACPCompiler
import build_setup

from wbe_utils_shader_compiler import WBEUtilsShaderCompiler


# The target profile and capabilities every shader compiles with.
SHADER_PROFILE = "spirv_1_6+spvDescriptorHeapEXT+vk_mem_model"


class ACPCompilerShader(ACPCompiler):
    """The shader compiler.

    Resolves a shader resource's source and compiles it with wbe-utils-shader-compiler, configured for the build target.
    """
    def __init__(self, shader_include_dirs: list[Path], generated_shader_dir: Path) -> None:
        """Constructor.

        Args:
            shader_include_dirs: The include directories for the shaders, in search order. Each directory's `utils`
                subdirectory is searched right after it.
            generated_shader_dir: The directory receiving the entry points generated for kernel resources.
        """
        search_paths = [path for include_dir in shader_include_dirs for path in (include_dir, include_dir / "utils")]
        macro_definitions = [("WBE_SHADER_DEBUG", "1")] if build_setup.build_target.get("enable_shader_debug", False) else []
        self._utils_shader_compiler = WBEUtilsShaderCompiler(
            profile=SHADER_PROFILE,
            include_dirs=search_paths,
            macro_definitions=macro_definitions,
            generated_source_dir=generated_shader_dir,
            debug_info=build_setup.build_target["generate-tests"],
        )

    @override
    def get_supported_resource_types(self) -> list[str]:
        return self._utils_shader_compiler.get_supported_resource_types()

    @override
    def compile(self, resource: ManifestResource, manifest_path: Path,
            res_dir: Path, res_output_dir: Path) -> list[ManifestResource]:
        resolved_resource = dict(resource)
        resolved_resource.setdefault("type", self._get_source_resource_type(resolved_resource) or "shader")
        resolved_resource.pop(SOURCE_RESOURCE_TYPE_KEY, None)
        source = resolved_resource.get("src")
        if not isinstance(source, str) or not source:
            raise RuntimeError(
                f"Shader resource must provide a src field in {manifest_path}: {resource}."
            )
        _, source_rel = self._resolve_resource_file_path(source, manifest_path, res_dir)
        resolved_resource["src"] = source_rel.as_posix()
        return self._utils_shader_compiler.compile(resolved_resource, manifest_path, res_dir, res_output_dir)
