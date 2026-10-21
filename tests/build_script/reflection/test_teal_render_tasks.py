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
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any
import unittest

from jinja2 import Environment, FileSystemLoader

from build_script.reflection.metadata_types import WBEClassMetadata, WBEFieldMetadata, WBEMetadata, WBEMethodMetadata
from build_script.reflection.teal_types import TEAL_FILTERS, teal_render_tasks
from build_script.resource.acp.acp_compiler_script import WBEACPCompilerScript

_ROOT = Path(__file__).resolve().parents[3]


def _params(p_name: str, p_fields: list[WBEFieldMetadata]) -> WBEClassMetadata:
    return WBEClassMetadata(name=p_name, attribute=["WBE", "WBE_REFLECT_SCRIPT"], fields=p_fields)


def _task(p_name: str, p_method: str | None, p_params: str, p_aliases: list[str] | None = None) -> WBEClassMetadata:
    return WBEClassMetadata(name=p_name, attribute=["WBE", "WBE_RENDER_TASK"],
                            attribute_parameters={} if p_method is None else {"WBE_RENDER_TASK": [p_method]},
                            aliases=["wire_" + str(p_method)] if p_aliases is None else p_aliases,
                            methods=[WBEMethodMetadata(method_name="create", is_static=True,
                                                       type=["const RenderContextVK *", f"const {p_params} &",
                                                             "const RendGraphContextVK &"])])


def _field(p_name: str, p_type: str, p_scope: str | None = None) -> WBEFieldMetadata:
    if p_scope is None:
        return WBEFieldMetadata(name=p_name, type=p_type, attribute=["WBE", "WBE_REFLECT"])
    return WBEFieldMetadata(name=p_name, type=p_type, attribute=["WBE", "WBE_REFLECT", "WBE_RECIPE_RESOURCE"],
                            attribute_parameters={"WBE_RECIPE_RESOURCE": [p_scope]})


def _describe(p_classes: list[WBEClassMetadata]) -> dict[str, Any]:
    return teal_render_tasks(WBEMetadata(classes=p_classes), ["WBE_RENDER_TASK"], ["WBE_REFLECT_SCRIPT"])


class TealRenderTasksTest(unittest.TestCase):
    def test_constructors_without_resource_fields_take_the_reflected_params(self) -> None:
        params = _params("RTParamsComputeVK", [_field("pipeline", "std::string")])
        api = _describe([params, _task("RTComputeVK", "compute", params.name)])
        self.assertEqual(api, {"interfaces": [], "constructors": [
            {"method": "compute", "type": "wire_compute", "params_type": "wbe.RTParamsComputeVK", "fields": []}]})
        api = _describe([params, _task("RTComputeVK", "compute", params.name, [])])
        self.assertEqual(api["constructors"][0]["type"], "RTComputeVK")

    def test_resource_fields_get_authoring_interfaces_and_descriptions(self) -> None:
        dispatch = _params("RTParamsDispatchVK", [
            _field("pipeline", "std::string"),
            _field("indirect_buffer", "std::string", "task_local"),
            _field("aliases", "DataValue", "task_local"),
        ])
        params = _params("RTParamsSequentialVK", [
            _field("dispatches", "std::vector<RTParamsDispatchVK>"),
            _field("views", "std::vector<std::string>", "graph"),
            _field("transform", "std::optional<std::string>", "graph"),
            _field("counts", "std::vector<uint32_t>"),
        ])
        api = _describe([params, dispatch, _task("RTSequentialVK", "compute_sequential", params.name)])
        task_local = {"kind": "resource", "scope": "task_local"}
        graph = {"kind": "resource", "scope": "graph"}
        self.assertEqual(api["constructors"][0]["params_type"], "RTParamsSequentialVK<Resource>")
        self.assertEqual(api["constructors"][0]["fields"], [
            {"name": "dispatches", "kind": "list", "element": {"kind": "record", "fields": [
                {"name": "indirect_buffer", **task_local},
                {"name": "aliases", "kind": "map", "element": task_local},
            ]}},
            {"name": "views", "kind": "list", "element": graph},
            {"name": "transform", **graph},
        ])
        interfaces = {interface["name"]: interface["fields"] for interface in api["interfaces"]}
        self.assertEqual(list(interfaces), ["RTParamsDispatchVK", "RTParamsSequentialVK"])
        self.assertEqual(interfaces["RTParamsSequentialVK"], [
            {"name": "dispatches", "type": "{RTParamsDispatchVK<Resource>}"},
            {"name": "views", "type": "{Resource | string}"},
            {"name": "transform", "type": "Resource | string"},
            {"name": "counts", "type": "{integer}"},
        ])
        self.assertEqual(interfaces["RTParamsDispatchVK"][2], {"name": "aliases", "type": "{string:Resource | string}"})

    def test_bare_render_task_has_no_constructor(self) -> None:
        params = _params("RTParamsComputeVK", [])
        self.assertEqual(_describe([params, _task("RTRegistryOnlyVK", None, params.name)]),
                         {"interfaces": [], "constructors": []})

    def test_invalid_constructor_names_fail_generation(self) -> None:
        params = _params("RTParamsComputeVK", [])
        for method in ['"compute"', "end", "two words", ""]:
            with self.subTest(method=method), self.assertRaisesRegex(ValueError, "one identifier"):
                _describe([params, _task("RTComputeVK", method, params.name)])
        with self.assertRaisesRegex(ValueError, "shadows"):
            _describe([params, _task("RTComputeVK", "chain", params.name)])
        with self.assertRaisesRegex(ValueError, "more than one"):
            _describe([params, _task("RTFirstVK", "compute", params.name), _task("RTSecondVK", "compute", params.name)])

    def test_invalid_tasks_and_resource_fields_fail_generation(self) -> None:
        params = _params("RTParamsComputeVK", [_field("buffer", "uint32_t", "graph")])
        task = _task("RTComputeVK", "compute", params.name)
        with self.assertRaisesRegex(ValueError, "std::string ids"):
            _describe([params, task])
        params.fields = [_field("buffer", "std::string", "task")]
        with self.assertRaisesRegex(ValueError, r"WBE_RECIPE_RESOURCE\(graph\)"):
            _describe([params, task])
        params.fields = []
        task.methods[0].type = task.methods[0].type[:2]
        with self.assertRaisesRegex(ValueError, "create"):
            _describe([params, task])
        with self.assertRaisesRegex(ValueError, "WBE_REFLECT_SCRIPT"):
            _describe([params, _task("RTComputeVK", "compute", "RTParamsHiddenVK")])
        nested = WBEClassMetadata(name="RTParamsHiddenVK", fields=[_field("buffer", "std::string", "graph")])
        params.fields = [_field("hidden", "RTParamsHiddenVK")]
        with self.assertRaisesRegex(ValueError, "WBE_REFLECT_SCRIPT"):
            _describe([params, nested, _task("RTComputeVK", "compute", params.name)])

    def test_generated_module_type_checks_and_packages_from_the_generated_directory(self) -> None:
        params = _params("RTParamsCopyVK", [_field("pipeline", "std::string"),
                                            _field("buffer", "std::optional<std::string>", "graph")])
        metadata = WBEMetadata(classes=[params, _task("RTCopyVK", "copy", params.name)])
        environment = Environment(loader=FileSystemLoader(_ROOT / "templates"), trim_blocks=True, lstrip_blocks=True)
        environment.filters.update(TEAL_FILTERS)
        module = environment.get_template("teal_render_graph_tasks.tl.jinja").render(
            metadata=metadata, render_tasks=["WBE_RENDER_TASK"], reflect_script=["WBE_REFLECT_SCRIPT"], _prefix="Test.")
        with TemporaryDirectory() as temporary:
            generated = Path(temporary) / "generated"
            (generated / "utils").mkdir(parents=True)
            (generated / "utils" / "render_graph_tasks.tl").write_text(module)
            (generated / "wbe.d.tl").write_text("local record wbe\nend\nreturn wbe\n")
            consumer = Path(temporary) / "consumer.tl"
            consumer.write_text('''local render_graph_tasks = require("utils.render_graph_tasks")
local record module
    record Resource
        id: string
    end
    record Task
        id: string
    end
    record Options
        uses: {Resource}
    end
    record CopyParams is render_graph_tasks.RTParamsCopyVK<Resource>
    end
    record Graph is render_graph_tasks.Graph<Resource, Task, Options, Graph>
        tasks: {Task}
    end
end
local graph: module.Graph = { tasks = {} }
render_graph_tasks.install(graph, function(p_id: string, _type: string, _params: any, _options: module.Options,
    _fields: {render_graph_tasks.ResourceField}): module.Task
    return { id = p_id }
end)
local params: module.CopyParams = { pipeline = "pipeline", buffer = { id = "buffer" } }
local task: module.Task = graph:copy("copy", params)
graph:copy("other", { pipeline = "pipeline", buffer = "buffer" })
return module
''')
            compiler = WBEACPCompilerScript(_ROOT / "dependencies" / "tl", [generated], generated)
            self.assertIn('p_make_task(p_id, "wire_copy", p_params, p_options, copy_fields)',
                          compiler._compile_teal(generated / "utils" / "render_graph_tasks.tl"))
            compiler._compile_teal(consumer)
            output = Path(temporary) / "output"
            resources = compiler.compile({"id": "utils.render_graph_tasks", "source_type": "script",
                                          "source_file": "utils/render_graph_tasks.tl",
                                          "path": "assets/scripts/teal/utils/render_graph_tasks.lua"},
                                         Path("lua_scripts.toml"), Path(temporary) / "res", output)
            self.assertEqual(resources[0]["path"], "assets/scripts/teal/utils/render_graph_tasks.lua")
            self.assertTrue((output / "assets/scripts/teal/utils/render_graph_tasks.lua").is_file())


if __name__ == "__main__":
    unittest.main()
