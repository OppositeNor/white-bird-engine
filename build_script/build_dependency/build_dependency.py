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
from __future__ import annotations

from abc import ABC, abstractmethod


class WBEBuildDependency(ABC):
    """A dependency that build.py compiles before running the resource pipeline and CMake."""

    @abstractmethod
    def get_name(self) -> str:
        """Return the name reported while compiling the dependency."""

    @abstractmethod
    def compile(self) -> None:
        """
        Configure and build the dependency.

        :raises RuntimeError: If the dependency fails to build.
        """


class WBEBuildDependencies:
    """Ordered set of build dependencies compiled as one build task."""

    def __init__(self) -> None:
        self._dependencies: list[WBEBuildDependency] = []

    def add(self, p_dependency: WBEBuildDependency) -> None:
        """Register a dependency; dependencies compile in registration order."""
        self._dependencies.append(p_dependency)

    def compile(self) -> None:
        """
        Compile every registered dependency.

        :raises RuntimeError: If any dependency fails to build.
        """
        for dependency in self._dependencies:
            print(f"WBEBuilder: Building dependency: {dependency.get_name()}...")
            dependency.compile()
