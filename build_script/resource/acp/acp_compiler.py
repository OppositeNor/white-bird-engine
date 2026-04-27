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
from typing import Any

ManifestResource = dict[str, Any]

class WBEACPCompiler(abc.ABC):
    """The compiler interface for the ACP."""
    @abc.abstractmethod
    def get_supported_resource_types(self) -> list[str]:
        """Get the resource types supported by this compiler.

        Returns:
            A list of strings of the resource types supported by this compiler.
        """
        pass

    @abc.abstractmethod
    def compile(self, resource: ManifestResource, manifest_path: Path,
                res_dir: Path, res_output_dir: Path) -> ManifestResource:
        """Compile or transform one resource from a manifest entry.

        Args:
            resource: Input resource manifest entry.
            manifest_path: Relative path of the manifest file that owns this entry.
            res_dir: Root source resource directory.
            res_output_dir: Root output resource directory.

        Returns:
            The normalized resource entry to write to generated runtime index files.
        """
        pass
