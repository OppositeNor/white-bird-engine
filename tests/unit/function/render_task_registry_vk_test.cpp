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
#include "function/render_vk/render_task_registry_vk.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/parser/parser_json.hh"
#include "core/parser/parser_toml.hh"
#include "core/script/script_value.hh"
#include "function/render/render_database.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "render_task_registry_vk_test_tasks.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

ScriptValue make_probe_params(std::string_view p_label, int64_t p_count) {
    ScriptValue params;
    params.set_entry("label", ScriptValue::make_string(p_label));
    params.set_entry("count", ScriptValue::make_integer(p_count));
    return params;
}

const RTRegistryProbeVK& as_probe(const Unique<IRenderTaskVK>& p_task) {
    const auto* probe = dynamic_cast<const RTRegistryProbeVK*>(p_task.get());
    EXPECT_NE(probe, nullptr);
    return *probe;
}

// The registry is instantiated for exactly these parameter types.
static_assert(RenderTaskParamsDataConcept<JSONData>);
static_assert(RenderTaskParamsDataConcept<TOMLData>);
static_assert(RenderTaskParamsDataConcept<ScriptValue>);
static_assert(!RenderTaskParamsDataConcept<int>);
static_assert(!RenderTaskParamsDataConcept<const JSONData&>);

} // namespace

TEST(WBERenderTaskRegistryVKTest, RegistersClassNamesAndAliases) {
    for (std::string_view name : {"RTRegistryProbeVK", "registry_probe", "registry_probe_alias", "RTRunComputePipelineVK", "run_compute_pipeline"}) {
        EXPECT_TRUE(RenderTaskRegistryVK::contains(name)) << name;
        const auto task_types = RenderTaskRegistryVK::get_task_types();
        EXPECT_NE(std::ranges::find(task_types, name), task_types.end()) << name;
    }
    EXPECT_FALSE(RenderTaskRegistryVK::contains("missing_task"));
    EXPECT_FALSE(RenderTaskRegistryVK::contains(""));
}

TEST(WBERenderTaskRegistryVKTest, FactoryCreatesTaskFromParameters) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    const RendTaskFactoryVK factory = RenderTaskRegistryVK::create_factory("registry_probe", make_probe_params("bloom", 3), &render_data);
    ASSERT_TRUE(static_cast<bool>(factory));

    const Unique<IRenderTaskVK> task = factory(RendGraphContextVK{.swap_chain_extent = {.width = 640, .height = 480}});
    const RTRegistryProbeVK& probe = as_probe(task);
    EXPECT_EQ(probe.get_render_data(), &render_data);
    EXPECT_EQ(probe.get_params().label, "bloom");
    EXPECT_EQ(probe.get_params().count, 3U);
    EXPECT_EQ(probe.get_extent().width, 640U);
    EXPECT_EQ(probe.get_extent().height, 480U);

    // The graph re-evaluates the factory with a new context after a resize.
    const Unique<IRenderTaskVK> resized_task = factory(RendGraphContextVK{.swap_chain_extent = {.width = 1280, .height = 720}});
    EXPECT_EQ(as_probe(resized_task).get_extent().width, 1280U);
    EXPECT_EQ(as_probe(resized_task).get_params().label, "bloom");
}

TEST(WBERenderTaskRegistryVKTest, ClassNameAndAliasesCreateTheSameTaskType) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    for (std::string_view name : {"RTRegistryProbeVK", "registry_probe_alias"}) {
        const Unique<IRenderTaskVK> task =
            RenderTaskRegistryVK::create_factory(name, make_probe_params("named", 1), &render_data)(RendGraphContextVK{});
        EXPECT_EQ(as_probe(task).get_params().label, "named") << name;
    }
}

TEST(WBERenderTaskRegistryVKTest, AcceptsJSONParameters) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    ParserJSON parser;
    parser.parse_from_buffer(R"({"label": "from json", "count": 4})");
    const Unique<IRenderTaskVK> task =
        RenderTaskRegistryVK::create_factory("registry_probe", parser.get_data(), &render_data)(RendGraphContextVK{});
    EXPECT_EQ(as_probe(task).get_params().label, "from json");
    EXPECT_EQ(as_probe(task).get_params().count, 4U);
}

TEST(WBERenderTaskRegistryVKTest, AcceptsTOMLParameters) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    ParserTOML parser;
    parser.parse_from_buffer("label = \"from toml\"\ncount = 5\n");
    const Unique<IRenderTaskVK> task =
        RenderTaskRegistryVK::create_factory("registry_probe", parser.get_data(), &render_data)(RendGraphContextVK{});
    EXPECT_EQ(as_probe(task).get_params().label, "from toml");
    EXPECT_EQ(as_probe(task).get_params().count, 5U);

    ParserTOML missing_label;
    missing_label.parse_from_buffer("count = 5\n");
    EXPECT_THROW(RenderTaskRegistryVK::create_factory("registry_probe", missing_label.get_data(), &render_data), std::runtime_error);
}

TEST(WBERenderTaskRegistryVKTest, OptionalParametersKeepTheirDefaults) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    ScriptValue params;
    params.set_entry("label", ScriptValue::make_string("defaults"));
    const Unique<IRenderTaskVK> task = RenderTaskRegistryVK::create_factory("registry_probe", params, &render_data)(RendGraphContextVK{});
    EXPECT_EQ(as_probe(task).get_params().count, 1U);
}

TEST(WBERenderTaskRegistryVKTest, CreateFactoryRejectsInvalidRequests) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    EXPECT_THROW(RenderTaskRegistryVK::create_factory("missing_task", make_probe_params("x", 1), &render_data), std::runtime_error);
    EXPECT_THROW(RenderTaskRegistryVK::create_factory("registry_probe", make_probe_params("x", 1), nullptr), std::runtime_error);
    // The required label is missing, so deserialization fails before any factory exists.
    EXPECT_THROW(RenderTaskRegistryVK::create_factory("registry_probe", ScriptValue::make_map(), &render_data), std::runtime_error);
    // A count that does not fit the field's type is rejected while deserializing.
    EXPECT_THROW(RenderTaskRegistryVK::create_factory("registry_probe", make_probe_params("x", -1), &render_data), std::runtime_error);
}

TEST(WBERenderTaskRegistryVKTest, CreateFailuresSurfaceWhenTheFactoryRuns) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    const RendTaskFactoryVK factory = RenderTaskRegistryVK::create_factory("registry_probe", make_probe_params("x", 0), &render_data);
    EXPECT_THROW(factory(RendGraphContextVK{}), std::runtime_error);
}

TEST(WBERenderTaskRegistryVKTest, ComputePipelineTaskRequiresACompiledPipeline) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    ScriptValue params;
    params.set_entry("pipeline", ScriptValue::make_string("pipelines/missing"));
    params.set_entry("group_count_x", ScriptValue::make_integer(8));

    // Without a render database the pipeline cannot be looked up.
    const RendTaskFactoryVK factory = RenderTaskRegistryVK::create_factory("run_compute_pipeline", params, &render_data);
    EXPECT_THROW(factory(RendGraphContextVK{}), std::runtime_error);

    Ref<RenderDatabase> render_database = make_ref<RenderDatabase>(&allocator, &allocator);
    render_data.render_database = render_database.get();
    EXPECT_THROW(factory(RendGraphContextVK{}), std::runtime_error);
    render_data.render_database = nullptr;

    // The pipeline key is required.
    EXPECT_THROW(RenderTaskRegistryVK::create_factory("run_compute_pipeline", ScriptValue::make_map(), &render_data), std::runtime_error);
}

} // namespace WhiteBirdEngine
