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
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from build_script.resource.acp.acp import ACP
from build_script.resource.acp.manifest_loader_lua import LuaManifestLoader, get_manifest_stem


def _write(path: Path, text: str) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


class LuaManifestLoaderTest(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = TemporaryDirectory()
        self.root = Path(self._temporary.name)

    def tearDown(self) -> None:
        self._temporary.cleanup()

    def _load(self, source: str, helper_roots: list[Path] | None = None) -> object:
        _write(self.root / "test.manifest.lua", source)
        loader = LuaManifestLoader(self.root, helper_roots if helper_roots is not None else [self.root])
        return loader.evaluate("test.manifest.lua").data

    def test_converts_arrays_maps_and_scalars(self) -> None:
        data = self._load("""
            return { resources = { { id = "a", count = 3, scale = 1.0, enabled = true, tags = {} } }, b = { z = 1, a = 2 } }
        """)
        self.assertEqual(data, {"b": {"a": 2, "z": 1}, "resources": [
            {"count": 3, "enabled": True, "id": "a", "scale": 1.0, "tags": []}]})
        resource = data["resources"][0]
        self.assertIsInstance(resource["count"], int)
        self.assertIsInstance(resource["scale"], float)
        self.assertEqual(list(data["b"].keys()), ["a", "z"])

    def test_computed_declarations(self) -> None:
        data = self._load("""
            local resources = {}
            for _, bits in ipairs({ 0, 8 }) do
                resources[#resources + 1] = { id = string.format("pass_%d", bits), bits = bits }
            end
            return { resources = resources }
        """)
        self.assertEqual(data["resources"], [{"bits": 0, "id": "pass_0"}, {"bits": 8, "id": "pass_8"}])

    def test_sandbox_hides_impure_functions(self) -> None:
        data = self._load("""
            return { resources = {}, visible = {
                io = io ~= nil, os = os ~= nil, debug = debug ~= nil, package = package ~= nil, load = load ~= nil,
                dofile = dofile ~= nil, loadfile = loadfile ~= nil, random = math.random ~= nil,
                dump = string.dump ~= nil, python = python ~= nil,
            } }
        """)
        self.assertTrue(all(not visible for visible in data["visible"].values()), data["visible"])

    def test_manifests_do_not_share_globals_or_libraries(self) -> None:
        loader = LuaManifestLoader(self.root, [self.root])
        _write(self.root / "first.manifest.lua", "leaked = 1; string.leaked = 1; return { resources = {} }")
        _write(self.root / "second.manifest.lua", "return { resources = {}, seen = { leaked ~= nil, string.leaked ~= nil } }")
        loader.evaluate("first.manifest.lua")
        self.assertEqual(loader.evaluate("second.manifest.lua").data["seen"], [False, False])

    def test_require_of_a_manifest_records_includes_in_require_order(self) -> None:
        _write(self.root / "assets" / "b.manifest.lua", "return { resources = { { id = 'b_mesh', type = 't' } } }")
        _write(self.root / "assets" / "a.manifest.lua", "return { resources = {} }")
        _write(self.root / "root.manifest.lua", """
            local b = require("assets.b")
            require("assets.a")
            require("assets.b")
            if false then require("assets.missing") end
            return { resources = { { id = "uses_b", type = "t", mesh = b.id("b_mesh"), from = b.path } } }
        """)
        loader = LuaManifestLoader(self.root, [self.root])
        record = loader.evaluate("root.manifest.lua")
        self.assertEqual(record.includes, ["assets/b.manifest.lua", "assets/a.manifest.lua"])
        self.assertEqual(record.data["resources"][0]["mesh"], "b_mesh")
        self.assertEqual(record.data["resources"][0]["from"], "assets/b.manifest.lua")
        self.assertIs(loader.evaluate("assets/b.manifest.lua"), loader.evaluate("assets/b.manifest.lua"))

    def test_manifest_required_through_a_helper_is_an_include_of_the_caller(self) -> None:
        _write(self.root / "assets" / "shared.manifest.lua", "return { resources = {} }")
        _write(self.root / "manifest_utils" / "deps.lua",
               'local deps = {}\nfunction deps.shared() return require("assets.shared") end\nreturn deps')
        _write(self.root / "first.manifest.lua", 'require("manifest_utils.deps").shared(); return { resources = {} }')
        _write(self.root / "second.manifest.lua", 'require("manifest_utils.deps").shared(); return { resources = {} }')
        loader = LuaManifestLoader(self.root, [self.root])
        self.assertEqual(loader.evaluate("first.manifest.lua").includes, ["assets/shared.manifest.lua"])
        self.assertEqual(loader.evaluate("second.manifest.lua").includes, ["assets/shared.manifest.lua"])

    def test_handle_id_rejects_undeclared_resources(self) -> None:
        _write(self.root / "assets" / "b.manifest.lua", "return { resources = { { id = 'b_mesh', type = 't' } } }")
        with self.assertRaisesRegex(Exception, "declares no resource 'typo'"):
            self._load('return { resources = { { id = "x", type = "t", mesh = require("assets.b").id("typo") } } }')

    def test_require_of_a_missing_or_cyclic_manifest_fails(self) -> None:
        with self.assertRaisesRegex(Exception, "assets/missing.manifest.lua does not exist"):
            self._load('require("assets.missing"); return { resources = {} }')
        _write(self.root / "a.manifest.lua", 'require("b"); return { resources = {} }')
        _write(self.root / "b.manifest.lua", 'require("a"); return { resources = {} }')
        with self.assertRaisesRegex(Exception, "require cycle: a.manifest.lua -> b.manifest.lua -> a.manifest.lua"):
            LuaManifestLoader(self.root, [self.root]).evaluate("a.manifest.lua")

    def test_require_loads_helpers_once(self) -> None:
        _write(self.root / "manifest_utils" / "counter.lua", """
            local helper = {}
            function helper.pass(bits) return { id = "pass_" .. bits } end
            return helper
        """)
        data = self._load("""
            local first = require("manifest_utils.counter")
            local second = require("manifest_utils.counter")
            return { resources = { first.pass(8) }, same = first == second }
        """)
        self.assertEqual(data["resources"], [{"id": "pass_8"}])
        self.assertTrue(data["same"])

    def test_require_searches_helper_roots_in_order(self) -> None:
        project = self.root / "project"
        engine = self.root / "engine"
        _write(project / "manifest_utils" / "shared.lua", "return { origin = 'project' }")
        _write(engine / "manifest_utils" / "shared.lua", "return { origin = 'engine' }")
        _write(engine / "manifest_utils" / "engine_only.lua", "return { origin = 'engine' }")
        data = self._load("""
            return { resources = {}, origins = { require("manifest_utils.shared").origin,
                require("manifest_utils.engine_only").origin } }
        """, [project, engine])
        self.assertEqual(data["origins"], ["project", "engine"])

    def test_require_of_a_missing_helper_fails(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "was not found"):
            self._load('require("manifest_utils.missing"); return { resources = {} }')

    def test_cyclic_require_fails(self) -> None:
        _write(self.root / "manifest_utils" / "a.lua", 'return require("manifest_utils.b")')
        _write(self.root / "manifest_utils" / "b.lua", 'return require("manifest_utils.a")')
        with self.assertRaisesRegex(RuntimeError, "cyclic require"):
            self._load('require("manifest_utils.a"); return { resources = {} }')

    def test_rejects_tables_that_are_not_data(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "mixes array and map keys"):
            self._load('return { resources = { 1, x = 2 } }')
        with self.assertRaisesRegex(RuntimeError, "holes"):
            self._load('return { resources = { [1] = 1, [3] = 3 } }')
        with self.assertRaisesRegex(RuntimeError, "function"):
            self._load('return { resources = { print = function() end } }')

    def test_reports_lua_errors_with_the_manifest_path(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "test.manifest.lua"):
            self._load('error("broken manifest")')
        with self.assertRaisesRegex(RuntimeError, "test.manifest.lua"):
            self._load('return {')

    def test_manifest_stem_drops_the_manifest_suffix(self) -> None:
        self.assertEqual(get_manifest_stem(Path("assets/meshes/cube.manifest.lua")), "cube")


class ACPLuaManifestTest(unittest.TestCase):
    def test_root_and_included_manifests_emit_json_indices(self) -> None:
        with TemporaryDirectory() as temporary:
            res = Path(temporary) / "res"
            output = Path(temporary) / "output"
            _write(res / "res_chunks" / "root.manifest.lua", """
                require("assets.child")
                return { resources = {} }
            """)
            _write(res / "assets" / "child.manifest.lua", """
                return { resources = { { id = "value", type = "test_value", value = 2 },
                    { id = "hidden", type = "test_value", scope = "test_only" } } }
            """)
            _write(res / "assets" / "unreferenced.manifest.lua", "error('must not be evaluated')")
            ACP(res, output, enabled_test=False).process_resources()
            root = json.loads((output / "res_chunks" / "root.json").read_text(encoding="utf-8"))
            child = json.loads((output / "assets" / "child.json").read_text(encoding="utf-8"))
            self.assertEqual(root, {"resources": [], "include": ["assets/child.json"]})
            self.assertEqual(child, {"resources": [{"id": "value", "type": "test_value", "value": 2}], "include": []})

    def test_test_only_required_manifests_are_left_out_of_deployment_includes(self) -> None:
        with TemporaryDirectory() as temporary:
            res = Path(temporary) / "res"
            output = Path(temporary) / "output"
            _write(res / "res_chunks" / "root.manifest.lua", 'require("assets.tests"); return { resources = {} }')
            _write(res / "assets" / "tests.manifest.lua", 'return { scope = "test_only", resources = {} }')
            ACP(res, output, enabled_test=False).process_resources()
            root = json.loads((output / "res_chunks" / "root.json").read_text(encoding="utf-8"))
            self.assertEqual(root["include"], [])
            self.assertFalse((output / "assets" / "tests.json").exists())
            ACP(res, Path(temporary) / "test_output", enabled_test=True).process_resources()
            test_root = json.loads((Path(temporary) / "test_output" / "res_chunks" / "root.json").read_text(encoding="utf-8"))
            self.assertEqual(test_root["include"], ["assets/tests.json"])

if __name__ == "__main__":
    unittest.main()
