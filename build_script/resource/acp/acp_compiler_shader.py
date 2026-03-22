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
import subprocess
import shutil
from typing import override
from build_script.resource.acp.acp_compiler import WBEACPCompiler
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
    def get_supported_file_extensions(self) -> list[str]:
        return [".slang", ".hlsl"]

    @override
    def compile(self, res_path: Path, output_dir: Path):
        output_file_path = Path.joinpath(output_dir, Path(res_path.stem + ".spv"))
        os.makedirs(output_file_path.parent, exist_ok=True)
        if res_path.suffix == ".slang":
            self._compile_slang(res_path, output_file_path)
        elif res_path.suffix == ".hlsl":
            self._compile_glslc_hlsl(res_path, output_file_path)
        else:
            raise RuntimeError(f"Unsupported shader file type: {res_path.suffix}.")

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

    def _compile_slang(self, shader_file: Path, output_file: Path):
        shader_stage = self._read_shader_stage(shader_file)

        slangc_path = shutil.which("slangc")
        if slangc_path is not None:
            self._compile_slangc_slang(slangc_path, shader_file, output_file, shader_stage)
            return
        glslang_validator_path = shutil.which("glslangValidator")
        if glslang_validator_path is not None:
            self._compile_glslang_slang(glslang_validator_path, shader_file, output_file, shader_stage)
            return

        raise RuntimeError("Failed to find valid slang compiler.")

    def _compile_slangc_slang(self, slangc_path: str, shader_file: Path,
                              output_file: Path, shader_stage: str):
        shader_stage = self._read_shader_stage(shader_file)
        cmd = [
            slangc_path,
            "-target", "spirv",
            "-entry", "main",
            "-stage", shader_stage,
            "-I", str(self._shader_include_dir),
            "-o", str(output_file),
            str(shader_file)
        ]
        if build_setup.build_target["generate-tests"]:
            cmd.extend(["-g3"])
        print("Compiling:", shader_file)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Failed to compile", shader_file)
            print(result.stderr)
            raise RuntimeError("Failed to compile shader.")

    def _compile_glslang_slang(self, glslang_validator_path: str, shader_file: Path,
                               output_file: Path, shader_stage: str):
        shader_stage = self._normalize_stage_for_glslang(shader_stage)
        cmd = [
            glslang_validator_path,
            "-V",
            "-D",
            "-S", shader_stage,
            "-I" + str(self._shader_include_dir),
            "-e", "main",
            "-o", str(output_file),
            str(shader_file),
        ]
        if build_setup.build_target["generate-tests"]:
            cmd.append("-g")
        print("Compiling:", shader_file)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Failed to compile", shader_file)
            print(result.stderr)
            raise RuntimeError("Failed to compile shader.")

    def _normalize_stage_for_glslang(self, shader_stage: str) -> str:
        stage_alias_map = {
            "vertex": "vert",
            "vert": "vert",
            "fragment": "frag",
            "frag": "frag",
            "tesscontrol": "tesc",
            "tesc": "tesc",
            "tesseval": "tese",
            "tese": "tese",
            "geometry": "geom",
            "geom": "geom",
            "compute": "comp",
            "comp": "comp",
        }
        return stage_alias_map[shader_stage]

    def _compile_glslc_hlsl(self, shader_file: Path, output_file: Path):
        shader_stage = self._read_shader_stage(shader_file)

        cmd = [
            "glslc",
            "-x", "hlsl",
            f"-fshader-stage={shader_stage}",
            "-I", self._shader_include_dir,
            "-fentry-point=main",
            "-o", str(output_file),
            str(shader_file)
        ]
        if build_setup.build_target["generate-tests"]:
            cmd.append("-g")
        print("Compiling:", shader_file)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Failed to compile", shader_file)
            print(result.stderr)
            raise RuntimeError("Failed to compile shader.")

