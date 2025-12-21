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
from typing import override
from build_script.resource.acp.acp_compiler import WBEACPCompiler
import build_setup


class WBEACPCompilerShader(WBEACPCompiler):
    """The shader compiler.

    Attributes: 
        shader_include_dir: The include directory for the shaders.
    """
    def __init__(self, shader_include_dir : Path) -> None:
        """Constructor.

        Args:
            shader_include_dir: The include directory for the shaders.
        """
        self.shader_include_dir = os.path.join(build_setup.resource_dir, shader_include_dir)

    @override
    def get_supported_file_extensions(self) -> list[str]:
        return [".hlsl"]

    @override
    def compile(self, res_path : str):
        input_file_path = Path.joinpath(Path(build_setup.resource_dir), Path(res_path))
        output_file_path = Path.joinpath(Path(build_setup.resource_output_dir),
                                         Path(res_path[:res_path.rfind('.')] + ".spv"))
        os.makedirs(output_file_path.parent, exist_ok=True)
        if input_file_path.suffix == ".hlsl":
            self._compile_hlsl(input_file_path, output_file_path)

    def _compile_hlsl(self, hlsl_file : Path, output_file):
        with open(hlsl_file, "r", encoding="utf-8") as f:
            first_line = f.readline().strip()
        # The shader stage is marked at the beginning of the file starting with a "//"
        if first_line.startswith("//"):
            shader_stage = first_line[2:].strip()
            if shader_stage not in ["vertex", "vert", "fragment", "frag", "tesscontrol",
                    "tesc", "tesseval", "tese", "geometry", "geom", "compute", "comp"]:
                raise RuntimeError(f"Unsupported shader stage for {hlsl_file} : {shader_stage}.")
        else:
            raise RuntimeError(f"File {hlsl_file} missing a stage header.")

        cmd = [
            "glslc",
            f"-fshader-stage={shader_stage}",
            "-I", self.shader_include_dir,
            "-fentry-point=main",
            "-o", str(output_file),
            str(hlsl_file)
        ]
        print("Compiling:", hlsl_file)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Failed to compile", hlsl_file)
            print(result.stderr)
            raise RuntimeError("Failed to compile shader.")

