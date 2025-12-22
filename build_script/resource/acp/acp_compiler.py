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
from pathlib import Path

class WBEACPCompiler(abc.ABC):
    """The compiler interface for the ACP."""
    @abc.abstractmethod
    def get_supported_file_extensions(self) -> list[str]:
        """Get the file extensions that this compiler is able to compile. Note that the '.'
        is included, i.e., for PNG files it should be [".png"]

        Returns:
            A list of strings of the file extensions that this compiler is able to compile.
            The '.' is included.
        """
        pass

    @abc.abstractmethod
    def compile(self, res_path: Path, output_dir: Path) -> None:
        """Compile a file.

        Args:
            res_path: The res path of the file to be compiled.
            output_dir: The directory that the compiled resource will be export to.
        """
        pass
