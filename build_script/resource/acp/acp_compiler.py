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
import abc

class WBEACPCompiler(abc.ABC):
    """The compiler interface for the ACP."""
    @abc.abstractmethod
    def get_supported_file_extensions(self) -> list[str]:
        """Get the file extensions that this compiler is able to compile.

        Returns:
            A list of strings of the file extensions that this compiler is able to compile.
        """
        pass

    @abc.abstractmethod
    def get_output_file_res_path(self, res_path: str) -> str:
        """Get the output file's resource path after compiling.

        Args:
            res_path: The resource path of the input file that is going to be compiled.

        Returns:
            The output file's resource path.
        """
        pass

    @abc.abstractmethod
    def compile(self, res_path: str) -> None:
        """Compile a file.

        Args:
            res_path: The res path of the file to be compiled. The result file will be compiled and the extension will
            be replaced with the file type extension that this compiler will compile
        """
        pass
