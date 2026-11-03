# Copyright 2026 OppositeNor
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
"""Game projects built against the engine with `build.py --project <dir>`."""
import os
from dataclasses import dataclass
from pathlib import Path
import tomllib
from build_config import header_extensions
from build_script.utils import list_files

PROJECT_MANIFEST_NAME = "wbe_project.toml"


@dataclass
class WBEProject:
    """A game project built against the engine, described by its wbe_project.toml.

    Attributes:
        name: The project name.
        root_dir: The absolute directory of the project manifest. It must contain the project's CMakeLists.txt.
        include_dirs: Absolute public include directories. Their headers are reflected, and they are added to the engine
            library's public include directories.
        source_dirs: Absolute source directories. Their headers are reflected as well.
        resource_dir: The absolute resource directory, laid out like the engine's res directory, or None.
        gen_info_files: Absolute paths of the project's generate.toml files.
    """
    name: str
    root_dir: str
    include_dirs: list[str]
    source_dirs: list[str]
    resource_dir: str | None
    gen_info_files: list[str]

    def get_headers(self) -> list[str]:
        """Return the absolute paths of the project headers reflection scans."""
        headers: list[str] = []
        for directory in self.include_dirs + self.source_dirs:
            headers.extend(path for path in list_files(directory, ignore_dirs=["generated", "__pycache__"])
                           if Path(path).suffix in header_extensions)
        return headers


def load_project(p_project_dir: str) -> WBEProject:
    """Load the project whose wbe_project.toml is in p_project_dir.

    Raises:
        RuntimeError: If the manifest is missing or invalid.
    """
    project_root = os.path.abspath(p_project_dir)
    manifest_path = os.path.join(project_root, PROJECT_MANIFEST_NAME)
    if not os.path.isfile(manifest_path):
        raise RuntimeError(f"Project manifest {manifest_path} does not exist.")
    if not os.path.isfile(os.path.join(project_root, "CMakeLists.txt")):
        raise RuntimeError(f"Project {project_root} has no CMakeLists.txt.")
    with open(manifest_path, "rb") as manifest_file:
        manifest = tomllib.load(manifest_file)
    unknown_tables = set(manifest.keys()) - {"project"}
    if unknown_tables:
        raise RuntimeError(f"Project manifest {manifest_path} has unknown tables: {sorted(unknown_tables)}.")
    if "project" not in manifest:
        raise RuntimeError(f"Project manifest {manifest_path} has no [project] table.")
    project_table: dict[str, object] = manifest["project"]
    known_keys = {"name", "include_dirs", "source_dirs", "resource_dir", "generate_files"}
    unknown_keys = set(project_table.keys()) - known_keys
    if unknown_keys:
        raise RuntimeError(f"Project manifest {manifest_path} has unknown [project] keys: {sorted(unknown_keys)}.")
    if "name" not in project_table:
        raise RuntimeError(f"Project manifest {manifest_path} must set `name`.")
    name = project_table["name"]
    if not isinstance(name, str):
        raise RuntimeError(f"Project manifest {manifest_path}: `name` must be a string.")
    if not name:
        raise RuntimeError(f"Project manifest {manifest_path}: `name` must not be empty.")

    def resolve_dirs(p_key: str) -> list[str]:
        relative_dirs = project_table.get(p_key, [])
        if not isinstance(relative_dirs, list):
            raise RuntimeError(f"Project manifest {manifest_path}: `{p_key}` must be a list.")
        result: list[str] = []
        for relative_dir in relative_dirs:
            if not isinstance(relative_dir, str):
                raise RuntimeError(f"Project manifest {manifest_path}: `{p_key}` entry {relative_dir!r} must be a string.")
            directory = os.path.normpath(os.path.join(project_root, relative_dir))
            if not os.path.isdir(directory):
                raise RuntimeError(f"Project manifest {manifest_path}: `{p_key}` entry {relative_dir} is not a directory.")
            result.append(directory)
        return result

    resource_dir: str | None = None
    if "resource_dir" in project_table:
        relative_resource_dir = project_table["resource_dir"]
        if not isinstance(relative_resource_dir, str):
            raise RuntimeError(f"Project manifest {manifest_path}: `resource_dir` must be a string.")
        resource_dir = os.path.normpath(os.path.join(project_root, relative_resource_dir))
        if not os.path.isdir(resource_dir):
            raise RuntimeError(f"Project manifest {manifest_path}: `resource_dir` {relative_resource_dir} is not a directory.")

    generate_files = project_table.get("generate_files", [])
    if not isinstance(generate_files, list):
        raise RuntimeError(f"Project manifest {manifest_path}: `generate_files` must be a list.")
    gen_files: list[str] = []
    for relative_file in generate_files:
        if not isinstance(relative_file, str):
            raise RuntimeError(f"Project manifest {manifest_path}: `generate_files` entry {relative_file!r} must be a string.")
        gen_file = os.path.normpath(os.path.join(project_root, relative_file))
        if not os.path.isfile(gen_file):
            raise RuntimeError(f"Project manifest {manifest_path}: `generate_files` entry {relative_file} does not exist.")
        gen_files.append(gen_file)

    return WBEProject(name=name, root_dir=project_root, include_dirs=resolve_dirs("include_dirs"),
                      source_dirs=resolve_dirs("source_dirs"), resource_dir=resource_dir, gen_info_files=gen_files)
