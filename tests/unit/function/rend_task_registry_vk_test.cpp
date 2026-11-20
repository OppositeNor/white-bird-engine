/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "fake_rend_graph.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "function/rend_vk/rend_task_registry_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "rend_task_registry_vk_test_tasks.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

DataValue make_probe_params(std::string_view p_label, int64_t p_count) {
    DataValue params;
    params.set_entry("label", DataValue::make_string(p_label));
    params.set_entry("count", DataValue::make_integer(p_count));
    return params;
}

const RTRegistryProbeVK& as_probe(const Unique<IRendTaskVK>& p_task) {
    const auto* probe = dynamic_cast<const RTRegistryProbeVK*>(p_task.get());
    EXPECT_NE(probe, nullptr);
    return *probe;
}

} // namespace

TEST(WBERendTaskRegistryVKTest, RegistersClassNamesAndAliases) {
    for (std::string_view name :
        {"RTRegistryProbeVK", "registry_probe", "registry_probe_alias", "RTRunComputePipelineVK", "run_compute_pipeline"}) {
        EXPECT_TRUE(RendTaskRegistryVK::contains(name)) << name;
        const auto task_types = RendTaskRegistryVK::get_task_types();
        EXPECT_NE(std::ranges::find(task_types, name), task_types.end()) << name;
    }
    EXPECT_FALSE(RendTaskRegistryVK::contains("missing_task"));
    EXPECT_FALSE(RendTaskRegistryVK::contains(""));
}

TEST(WBERendTaskRegistryVKTest, FactoryCreatesTaskFromParameters) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    const RendTaskFactoryVK factory = RendTaskRegistryVK::create_factory("registry_probe", make_probe_params("bloom", 3), &rend_data);
    ASSERT_TRUE(static_cast<bool>(factory));

    const Unique<IRendTaskVK> task = factory(RendGraphContextVK{.swap_chain_extent = {.width = 640, .height = 480}});
    const RTRegistryProbeVK& probe = as_probe(task);
    EXPECT_EQ(probe.get_rend_data(), &rend_data);
    EXPECT_EQ(probe.get_params().label, "bloom");
    EXPECT_EQ(probe.get_params().count, 3U);
    EXPECT_EQ(probe.get_extent().width, 640U);
    EXPECT_EQ(probe.get_extent().height, 480U);

    // The graph re-evaluates the factory with a new context after a resize.
    const Unique<IRendTaskVK> resized_task = factory(RendGraphContextVK{.swap_chain_extent = {.width = 1280, .height = 720}});
    EXPECT_EQ(as_probe(resized_task).get_extent().width, 1280U);
    EXPECT_EQ(as_probe(resized_task).get_params().label, "bloom");
}

TEST(WBERendTaskRegistryVKTest, ClassNameAndAliasesCreateTheSameTaskType) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    for (std::string_view name : {"RTRegistryProbeVK", "registry_probe_alias"}) {
        const Unique<IRendTaskVK> task =
            RendTaskRegistryVK::create_factory(name, make_probe_params("named", 1), &rend_data)(RendGraphContextVK{});
        EXPECT_EQ(as_probe(task).get_params().label, "named") << name;
    }
}

TEST(WBERendTaskRegistryVKTest, OptionalParametersKeepTheirDefaults) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    DataValue params;
    params.set_entry("label", DataValue::make_string("defaults"));
    const Unique<IRendTaskVK> task =
        RendTaskRegistryVK::create_factory("registry_probe", params, &rend_data)(RendGraphContextVK{});
    EXPECT_EQ(as_probe(task).get_params().count, 1U);
}

TEST(WBERendTaskRegistryVKTest, CreateFactoryRejectsInvalidRequests) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    EXPECT_THROW(RendTaskRegistryVK::create_factory("missing_task", make_probe_params("x", 1), &rend_data), std::runtime_error);
    EXPECT_THROW(RendTaskRegistryVK::create_factory("registry_probe", make_probe_params("x", 1), nullptr), std::runtime_error);
    // The required label is missing, so deserialization fails before any factory exists.
    EXPECT_THROW(RendTaskRegistryVK::create_factory("registry_probe", DataValue::make_map(), &rend_data), std::runtime_error);
    // A count that does not fit the field's type is rejected while deserializing.
    EXPECT_THROW(RendTaskRegistryVK::create_factory("registry_probe", make_probe_params("x", -1), &rend_data), std::runtime_error);
}

TEST(WBERendTaskRegistryVKTest, RejectsMisspeltParameters) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    DataValue params = make_probe_params("x", 1);
    params.set_entry("cuont", DataValue::make_integer(2));
    EXPECT_THROW(RendTaskRegistryVK::create_factory("registry_probe", params, &rend_data), std::runtime_error);

    DataValue compute_params;
    compute_params.set_entry("pipeline", DataValue::make_string("pipelines/blur"));
    compute_params.set_entry("group_count", DataValue::make_integer(8));
    EXPECT_THROW(RendTaskRegistryVK::create_factory("run_compute_pipeline", compute_params, &rend_data), std::runtime_error);
}

TEST(WBERendTaskRegistryVKTest, CreateFailuresSurfaceWhenTheFactoryRuns) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    const RendTaskFactoryVK factory = RendTaskRegistryVK::create_factory("registry_probe", make_probe_params("x", 0), &rend_data);
    EXPECT_THROW(factory(RendGraphContextVK{}), std::runtime_error);
}

TEST(WBERendTaskRegistryVKTest, ComputePipelineTaskRequiresACompiledPipeline) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    DataValue params;
    params.set_entry("pipeline", DataValue::make_string("pipelines/missing"));
    params.set_entry("group_count_x", DataValue::make_integer(8));

    // Without a render graph the pipeline cannot be looked up.
    const RendTaskFactoryVK factory = RendTaskRegistryVK::create_factory("run_compute_pipeline", params, &rend_data);
    EXPECT_THROW(factory(RendGraphContextVK{}), std::runtime_error);

    // A graph that registered no pipeline under the key fails the lookup too.
    FakeRendGraph rend_graph(&alloc);
    const RendTaskFactoryVK graph_factory =
        RendTaskRegistryVK::create_factory("run_compute_pipeline", params, &rend_data, &rend_graph);
    try {
        (void)graph_factory(RendGraphContextVK{});
        ADD_FAILURE() << "created a task whose pipeline is not registered.";
    } catch (const std::runtime_error& p_error) {
        EXPECT_NE(std::string(p_error.what()).find("no compute pipeline is registered on the render graph under 'pipelines/missing'"),
            std::string::npos)
            << p_error.what();
    }

    // The pipeline key is required.
    EXPECT_THROW(RendTaskRegistryVK::create_factory("run_compute_pipeline", DataValue::make_map(), &rend_data), std::runtime_error);
}

TEST(WBERendTaskRegistryVKTest, AddTaskCallsPrepareGraphAndAddsTheTask) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK graph(&alloc);
    RendTaskDescVK& first =
        RendTaskRegistryVK::add_task(graph, "first", "registry_preparing_probe", make_probe_params("shared", 1), &rend_data);
    EXPECT_EQ(first.task_id, "first");
    EXPECT_EQ(first.resource_map.resolve(RTRegistryPreparingProbeVK::INPUT_ID), "shared.prepared");
    // A second task reuses the buffer the first one declared.
    RendTaskRegistryVK::add_task(graph, "second", "registry_preparing_probe", make_probe_params("shared", 1), &rend_data);
    ASSERT_EQ(graph.tasks.size(), 2U);
    EXPECT_EQ(graph.buffers.size(), 1U);
    EXPECT_TRUE(graph.contains_resource("shared.prepared"));
    EXPECT_FALSE(graph.contains_resource("other.prepared"));
    EXPECT_NE(graph.get_task("second").factory, nullptr);
}

TEST(WBERendTaskRegistryVKTest, AddTaskWithoutPrepareGraphOnlyAddsTheTask) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK graph(&alloc);
    RendTaskDescVK& task = RendTaskRegistryVK::add_task(graph, "probe", "registry_probe", make_probe_params("plain", 2), &rend_data);
    EXPECT_TRUE(graph.buffers.empty());
    const Unique<IRendTaskVK> created = task.factory(RendGraphContextVK{});
    EXPECT_EQ(as_probe(created).get_params().count, 2U);
}

TEST(WBERendTaskRegistryVKTest, AddTaskRejectsInvalidRequests) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK graph(&alloc);
    RendTaskRegistryVK::add_task(graph, "probe", "registry_probe", make_probe_params("x", 1), &rend_data);
    EXPECT_THROW(RendTaskRegistryVK::add_task(graph, "probe", "registry_probe", make_probe_params("x", 1), &rend_data),
        std::runtime_error);
    EXPECT_THROW(RendTaskRegistryVK::add_task(graph, "", "registry_probe", make_probe_params("x", 1), &rend_data), std::runtime_error);
    EXPECT_THROW(RendTaskRegistryVK::add_task(graph, "other", "missing_task", make_probe_params("x", 1), &rend_data), std::runtime_error);
    EXPECT_THROW(RendTaskRegistryVK::add_task(graph, "other", "registry_probe", make_probe_params("x", 1), nullptr), std::runtime_error);
    EXPECT_THROW(RendTaskRegistryVK::add_task(graph, "other", "registry_probe", DataValue::make_map(), &rend_data), std::runtime_error);
    EXPECT_EQ(graph.tasks.size(), 1U);
}

} // namespace WhiteBirdEngine
