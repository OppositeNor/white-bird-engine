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

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import lupa.lua55

SOURCE_MANIFEST_SUFFIX = ".manifest.lua"
MANIFEST_HELPER_PACKAGE = "manifest_utils"

# Runs a chunk in a sandbox: only the pure standard functions are visible, the libraries are copies so a chunk cannot
# change another's, and require loads helper modules through the host's resolver, once per loader, and hands every other
# name to the host's manifest require.
_SANDBOX_SOURCE = r"""
local resolve_module, require_manifest, set_scope = ...
local load = load

local function copy(source, excluded)
    local result = {}
    for key, value in pairs(source) do
        if excluded == nil or not excluded[key] then
            result[key] = value
        end
    end
    return result
end

local base = {
    assert = assert, error = error, ipairs = ipairs, next = next, pairs = pairs, pcall = pcall, rawequal = rawequal,
    rawget = rawget, rawlen = rawlen, rawset = rawset, select = select, setmetatable = setmetatable,
    getmetatable = getmetatable, tonumber = tonumber, tostring = tostring, type = type, xpcall = xpcall,
    _VERSION = _VERSION,
}
local libraries = {
    math = copy(math, { random = true, randomseed = true }),
    string = copy(string, { dump = true }),
    table = copy(table),
    utf8 = copy(utf8),
}

local modules = {}
local loading = {}

local function make_environment(require_function)
    local environment = copy(base)
    for name, library in pairs(libraries) do
        environment[name] = copy(library)
    end
    environment.require = require_function
    environment.scope = set_scope
    environment._G = environment
    return environment
end

local function run(source, chunk_name, require_function)
    local environment = make_environment(require_function)
    local chunk, message = load(source, "@" .. chunk_name, "t", environment)
    if chunk == nil then
        error(message, 0)
    end
    return chunk()
end

local require_module
require_module = function(name)
    if type(name) ~= "string" then
        error("require expects a module name string", 2)
    end
    if name:sub(1, #"manifest_utils.") ~= "manifest_utils." then
        return require_manifest(name)
    end
    if modules[name] ~= nil then
        return modules[name]
    end
    if loading[name] then
        error("cyclic require of manifest helper module '" .. name .. "'", 2)
    end
    local source, chunk_name = resolve_module(name)
    if source == nil then
        error(chunk_name, 2)
    end
    loading[name] = true
    local result = run(source, chunk_name, require_module)
    loading[name] = nil
    if result == nil then
        result = true
    end
    modules[name] = result
    return result
end

return function(source, chunk_name)
    return run(source, chunk_name, require_module)
end
"""


@dataclass
class ManifestRecord:
    """An evaluated manifest.

    Attributes:
        data: The manifest's ``resources`` array, the converted value it returns, and its ``scope`` when it sets one.
        includes: Resource-root-relative paths of the manifests it requires, in the order of their first require.
        resource_ids: The ids of its top-level resources.
    """
    data: dict[str, Any]
    includes: list[str] = field(default_factory=list)
    resource_ids: set[str] = field(default_factory=set)


class LuaManifestLoader:
    """Evaluates resource manifests written in Lua in a sandbox, as the plain data the ACP processes.

    A manifest ``<path>.manifest.lua`` is a Lua chunk returning its array of resources. Calling ``scope(<scope>)`` sets
    the manifest's scope, which also applies to the manifests it requires. The
    sandbox exposes only the pure standard library: no ``io``, ``os``, ``debug``, ``package``, ``load``, ``dofile``,
    ``math.random``, or ``string.dump``, so evaluating a manifest is deterministic.

    ``require`` takes two kinds of names. ``require("manifest_utils.<name>")`` loads the helper module
    ``manifest_utils/<name>.lua`` from the first helper root that has it, in the same sandbox, once per loader. Any other
    dotted name is a manifest under the resource root (``assets.meshes.cube`` is ``assets/meshes/cube.manifest.lua``):
    the loader evaluates it once, records it as an include of the manifest being evaluated (also when a helper that
    manifest calls makes the require), and returns a handle whose ``path`` is the manifest's path and whose
    ``id(name)`` returns ``name`` after checking that the manifest declares a top-level resource with that id. A
    helper should require manifests only inside its functions, since its module body runs once, for whichever manifest
    first requires it. Cyclic requires fail.

    Returned tables become Python data: a table whose keys are exactly 1..n is a list, an empty table is an empty list,
    a table whose keys are all strings is a dict with sorted keys (Lua's iteration order is not stable), and any other
    table, function, or userdata is an error. Lua integers stay ints and floats stay floats.
    """

    def __init__(self, res_dir: Path, helper_roots: list[Path]) -> None:
        """Create a loader.

        Args:
            res_dir: The resource directory manifests are resolved under.
            helper_roots: Resource directories searched in order for ``manifest_utils`` helper modules.
        """
        self._res_dir = res_dir
        self._helper_roots = list(helper_roots)
        self._records: dict[str, ManifestRecord] = {}
        self._evaluating: list[str] = []
        self._pending_includes: dict[str, list[str]] = {}
        self._pending_scopes: dict[str, Any] = {}
        self._lua = lupa.lua55.LuaRuntime(unpack_returned_tuples=True, register_eval=False, register_builtins=False)
        self._run = self._lua.execute(_SANDBOX_SOURCE, self._resolve_module, self._require_manifest, self._set_scope)

    def evaluate(self, manifest_rel_path: str) -> ManifestRecord:
        """Evaluate a manifest, and every manifest it requires, once.

        Args:
            manifest_rel_path: The manifest's resource-root-relative path.

        Returns:
            The manifest's record.
        """
        record = self._records.get(manifest_rel_path)
        if record is not None:
            return record
        if manifest_rel_path in self._evaluating:
            chain = " -> ".join(self._evaluating[self._evaluating.index(manifest_rel_path):] + [manifest_rel_path])
            raise RuntimeError(f"WBEACP: Manifest require cycle: {chain}.")
        manifest_path = self._res_dir / manifest_rel_path
        if not manifest_path.is_file():
            raise RuntimeError(f"WBEACP: Manifest does not exist: {manifest_rel_path}.")
        source = manifest_path.read_text(encoding="utf-8")
        self._evaluating.append(manifest_rel_path)
        self._pending_includes[manifest_rel_path] = []
        try:
            result = self._run(source, manifest_rel_path)
        except lupa.lua55.LuaError as error:
            raise RuntimeError(f"WBEACP: Failed to evaluate Lua manifest {manifest_rel_path}: {error}.") from error
        finally:
            self._evaluating.pop()
            includes = self._pending_includes.pop(manifest_rel_path)
            scope = self._pending_scopes.pop(manifest_rel_path, None)
        resources = self._to_python(result, manifest_rel_path, "<manifest>")
        if not isinstance(resources, list):
            raise RuntimeError(f"WBEACP: Manifest {manifest_rel_path} must return an array of resources.")
        data: dict[str, Any] = {"resources": resources}
        if scope is not None:
            data["scope"] = scope
        resource_ids = {resource["id"] for resource in resources
                        if isinstance(resource, dict) and isinstance(resource.get("id"), str)}
        record = ManifestRecord(data=data, includes=includes, resource_ids=resource_ids)
        self._records[manifest_rel_path] = record
        return record

    def _set_scope(self, scope: Any) -> None:
        manifest_rel_path = self._evaluating[-1]
        if manifest_rel_path in self._pending_scopes:
            raise RuntimeError(f"WBEACP: Manifest {manifest_rel_path} sets its scope more than once.")
        self._pending_scopes[manifest_rel_path] = scope

    def _require_manifest(self, name: str) -> Any:
        requirer = self._evaluating[-1]
        parts = name.split(".")
        if not all(part.isidentifier() for part in parts):
            raise RuntimeError(f"WBEACP: {requirer} requires '{name}', which is not a dotted manifest name.")
        path = "/".join(parts) + SOURCE_MANIFEST_SUFFIX
        if not (self._res_dir / path).is_file():
            raise RuntimeError(f"WBEACP: {requirer} requires manifest '{name}', but {path} does not exist.")
        record = self.evaluate(path)
        includes = self._pending_includes[requirer]
        if path not in includes:
            includes.append(path)
        return self._make_handle(path, record)

    def _make_handle(self, path: str, record: ManifestRecord) -> Any:
        def resource_id(resource_name: str) -> str:
            if resource_name not in record.resource_ids:
                raise RuntimeError(f"WBEACP: Manifest {path} declares no resource '{resource_name}'.")
            return resource_name
        return self._lua.table_from({"path": path, "id": resource_id})

    def _resolve_module(self, name: str) -> tuple[str | None, str]:
        parts = name.split(".")
        if len(parts) < 2 or parts[0] != MANIFEST_HELPER_PACKAGE or not all(part.isidentifier() for part in parts):
            return None, f"manifest helper module '{name}' must be named {MANIFEST_HELPER_PACKAGE}.<name>"
        relative = Path(*parts).with_suffix(".lua")
        for root in self._helper_roots:
            candidate = root / relative
            if candidate.is_file():
                return candidate.read_text(encoding="utf-8"), relative.as_posix()
        return None, f"manifest helper module '{name}' was not found ({relative.as_posix()})"

    def _to_python(self, value: Any, manifest_rel_path: str, location: str) -> Any:
        if value is None or isinstance(value, (bool, int, float, str)):
            return value
        if lupa.lua55.lua_type(value) != "table":
            raise RuntimeError(
                f"WBEACP: Lua manifest {manifest_rel_path} returns a {lupa.lua55.lua_type(value)} at {location}; "
                "manifests may hold only tables, strings, numbers, and booleans."
            )
        keys = list(value.keys())
        if not keys:
            return []
        if all(isinstance(key, int) and not isinstance(key, bool) for key in keys):
            count = len(keys)
            if sorted(keys) != list(range(1, count + 1)):
                raise RuntimeError(
                    f"WBEACP: Lua manifest {manifest_rel_path} has an array with holes or non-positive indices at {location}."
                )
            return [self._to_python(value[index], manifest_rel_path, f"{location}[{index}]") for index in range(1, count + 1)]
        if all(isinstance(key, str) for key in keys):
            return {
                key: self._to_python(value[key], manifest_rel_path, f"{location}.{key}") for key in sorted(keys)
            }
        raise RuntimeError(
            f"WBEACP: Lua manifest {manifest_rel_path} mixes array and map keys in one table at {location}."
        )


def get_manifest_stem(manifest_path: Path) -> str:
    """Get a manifest's name without its manifest suffix (``cube.manifest.lua`` is ``cube``).

    Args:
        manifest_path: A source manifest path.

    Returns:
        The file name without ``.manifest.lua``.
    """
    name = manifest_path.name
    if name.endswith(SOURCE_MANIFEST_SUFFIX):
        return name[:-len(SOURCE_MANIFEST_SUFFIX)]
    return manifest_path.stem
