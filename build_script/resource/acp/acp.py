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
from build_script.resource.acp.acp_compiler import WBEACPCompiler

class WBEACP:
    """The asset conditioning pipeline.
    The ASP will gather all the resource chunks in res, and process them one-by-one through
    the compiler, and output to the corresponding file in the output directory of resources.
    """
    def __init__(self, res_dir: Path, res_output_dir: Path):
        self._res_dir = res_dir
        self._res_output_dir = res_output_dir
        self._compilers : list[WBEACPCompiler] = []

    def add_compiler(self, compiler):
        """Add a compiler that could compile resource.

        Args:
            compiler (): 
        """
        self._compilers.append(compiler)

    def process_resources(self):
        """Scan through the resource folder, and compile all files."""
        resource_files = list(Path(self._res_dir).rglob("*"))
        # Filter out all the directories.
        resource_files = [file for file in resource_files if file.is_file()]
        for res_file in resource_files:
            # Get the resource path, and convert to posix path since this is used as the
            # resource path format.
            is_compiled = False
            for compiler in self._compilers:
                if res_file.suffix in compiler.get_supported_file_extensions():
                    output_file_dir = Path.joinpath(self._res_output_dir, res_file.parent.relative_to(self._res_dir))
                    # Found suitable compiler for resource, compile.
                    compiler.compile(res_file, output_file_dir)
                    is_compiled = True
                    break
            if not is_compiled:
                relative_res_path = res_file.relative_to(self._res_dir)
                print(f'WBEACP: No suitable compiler found for {relative_res_path}, copied directly.')
                # If no compiler supports this format, copy the file directly to the output path.
                output_file = Path.joinpath(self._res_output_dir, relative_res_path)
                os.makedirs(output_file.parent, exist_ok=True)
                shutil.copyfile(res_file, output_file)
