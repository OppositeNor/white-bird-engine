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
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/rend_graph_recipe_builder_vk.hh"
#include "function/render_vk/rend_graph_script_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "global/global.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "test_global.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

constexpr std::string_view TEST_RECIPE_SCRIPT = R"(
local exports = {}

function exports.build(context)
    local tasks = {}
    for i = 1, context.frames_in_flight do
        tasks[#tasks + 1] = { id = "probe_" .. i, type = "registry_probe", params = { label = context.swap_chain_format } }
    end
    tasks[1].connect = { { to = "probe_2", resource = "shared" } }
    return {
        resources = { { id = "shared", sampler = { min_filter = "nearest" } } },
        tasks = tasks,
    }
end

return exports
)";

} // namespace

TEST(WBERendGraphScriptVKTest, MakesContextFromTheRenderData) {
    RenderAllocator allocator(WBE_KI_B(16));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 3;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    DataValue context = RendGraphScriptVK::make_context(render_data);
    EXPECT_EQ(context.get_value<uint32_t>("frames_in_flight"), 3U);
    EXPECT_EQ(context.get_value<std::string>("swap_chain_format"), "B8G8R8A8_SRGB");
    // A format ImageFormatVK has no name for is passed as its value.
    render_data.swap_chain_format = VK_FORMAT_R4G4_UNORM_PACK8;
    context = RendGraphScriptVK::make_context(render_data);
    EXPECT_EQ(context.get_value<int64_t>("swap_chain_format"), static_cast<int64_t>(VK_FORMAT_R4G4_UNORM_PACK8));
}

TEST(WBERendGraphScriptVKTest, RunsBuildAndTheRecipeBuilds) {
    RenderAllocator allocator(WBE_KI_B(256));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_UNORM;
    const DataValue recipe = RendGraphScriptVK::run("test_recipe", TEST_RECIPE_SCRIPT, RendGraphScriptVK::make_context(render_data));
    RendGraphDescVK graph = RendGraphRecipeBuilderVK().build(recipe, RendGraphRecipeImportsVK{}, &render_data);
    ASSERT_EQ(graph.tasks.size(), 2U);
    EXPECT_EQ(graph.get_task("probe_1").next.size(), 1U);
    EXPECT_EQ(graph.samplers.size(), 1U);
}

TEST(WBERendGraphScriptVKTest, RejectsScriptsWithoutBuild) {
    EXPECT_THROW((void)RendGraphScriptVK::run("no_build", "return {}", DataValue::make_map()), std::runtime_error);
    EXPECT_THROW((void)RendGraphScriptVK::run("broken", "return {", DataValue::make_map()), std::runtime_error);
}

class WBERendGraphScriptVKResourceTest : public ::testing::Test {
protected:
    virtual void SetUp() override {
        global = make_test_global();
        ResourceManager* resource_manager = ResourceManager::get_singleton();
        resource_manager->register_chunk(make_ref<ResourceChunkLoadFile>(
            global_allocator(), "assets/scripts/lua/lua_scripts.json", resource_manager->get_allocator()));
    }

    virtual void TearDown() override {
        global.reset();
    }

    std::unique_ptr<Global> global;
};

TEST_F(WBERendGraphScriptVKResourceTest, ScriptsRequireScriptResources) {
    const DataValue recipe = RendGraphScriptVK::run("requiring_recipe",
        R"(
local math_utils = require("utils.math")
return {
    build = function()
        return { imports = { tostring(math_utils.ceil_div(42, 5)) } }
    end,
}
)",
        DataValue::make_map());
    EXPECT_EQ(recipe.get_entry("imports").as_list().at(0).as_string(), "9");
    EXPECT_THROW((void)RendGraphScriptVK::run("missing", "require('utils.missing') return {}", DataValue::make_map()), std::runtime_error);
}

TEST_F(WBERendGraphScriptVKResourceTest, RunsTheShippedPixelSortingRecipe) {
    RenderAllocator allocator(WBE_KI_B(16));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    DataValue context = RendGraphScriptVK::make_context(render_data);
    context.set_value("source_width", 64);
    context.set_value("source_height", 32);
    context.set_value("orientation", std::string("horizontal"));
    const DataValue recipe = RendGraphScriptVK::run("render_graphs.pixel_sorting", context);
    // 3 line passes, the dispatch preparation, 5 tasks for each of the 4 radix sort passes, 2 image passes, and present.
    EXPECT_EQ(recipe.get_entry("tasks").as_list().size(), 27U);
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 12U);
}

} // namespace WhiteBirdEngine
