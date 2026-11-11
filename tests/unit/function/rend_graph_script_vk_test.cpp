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
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "global/global.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "test_global.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
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

// The resource of p_recipe whose debug name is p_name, of kind p_kind when it is set, since an image and its default view
// share their name.
const DataValue& find_named_resource(const DataValue& p_recipe, std::string_view p_name, std::string_view p_kind = {}) {
    for (const DataValue& resource : p_recipe.get_entry("resources").as_list()) {
        if (resource.contains("name") && resource.get_value<std::string>("name") == p_name &&
            (p_kind.empty() || resource.get_value<std::string>("id").starts_with(std::format("{}-", p_kind)))) {
            return resource;
        }
    }
    throw std::runtime_error(std::format("The recipe declares no resource named '{}'.", p_name));
}

// The generated id of the resource of p_recipe whose debug name is p_name; see find_named_resource().
std::string named_id(const DataValue& p_recipe, std::string_view p_name, std::string_view p_kind = {}) {
    return find_named_resource(p_recipe, p_name, p_kind).get_value<std::string>("id");
}

// The task of p_recipe whose debug name is p_name.
const DataValue& find_named_task(const DataValue& p_recipe, std::string_view p_name) {
    for (const DataValue& task : p_recipe.get_entry("tasks").as_list()) {
        if (task.contains("name") && task.get_value<std::string>("name") == p_name) {
            return task;
        }
    }
    throw std::runtime_error(std::format("The recipe has no task named '{}'.", p_name));
}

// The generated id of the task of p_recipe whose debug name is p_name.
std::string named_task_id(const DataValue& p_recipe, std::string_view p_name) {
    return find_named_task(p_recipe, p_name).get_value<std::string>("id");
}

// Whether p_id is a generated id of kind p_kind: `<kind>-` followed by 8 lowercase hexadecimal digits.
bool is_generated_id(std::string_view p_id, std::string_view p_kind) {
    if (p_id.size() != p_kind.size() + 9 || !p_id.starts_with(p_kind) || p_id[p_kind.size()] != '-') {
        return false;
    }
    return std::ranges::all_of(p_id.substr(p_kind.size() + 1), [](char p_digit) {
        return (p_digit >= '0' && p_digit <= '9') || (p_digit >= 'a' && p_digit <= 'f');
    });
}

} // namespace

TEST(WBERendGraphScriptVKTest, MakesContextFromTheRenderData) {
    RenderAllocator allocator(WBE_KI_B(16));
    RenderContextVK render_data(&allocator);
    render_data.frames_in_flight = 3;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    const RendGraphContextVK graph_context{.swap_chain_extent = {.width = 1920, .height = 1080}};
    DataValue context = RendGraphScriptVK::make_context(render_data, graph_context);
    EXPECT_EQ(context.get_value<uint32_t>("frames_in_flight"), 3U);
    EXPECT_EQ(context.get_value<std::string>("swap_chain_format"), "B8G8R8A8_SRGB");
    EXPECT_EQ(context.get_entry("swap_chain_extent").get_value<uint32_t>("width"), 1920U);
    EXPECT_EQ(context.get_entry("swap_chain_extent").get_value<uint32_t>("height"), 1080U);
    // A format ImageFormatVK has no name for is passed as its value.
    render_data.swap_chain_format = VK_FORMAT_R4G4_UNORM_PACK8;
    context = RendGraphScriptVK::make_context(render_data, graph_context);
    EXPECT_EQ(context.get_value<int64_t>("swap_chain_format"), static_cast<int64_t>(VK_FORMAT_R4G4_UNORM_PACK8));
}

TEST(WBERendGraphScriptVKTest, RunsBuildAndTheRecipeBuilds) {
    RenderAllocator allocator(WBE_KI_B(256));
    RenderContextVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_UNORM;
    const DataValue recipe = RendGraphScriptVK::run(
        "test_recipe", TEST_RECIPE_SCRIPT, RendGraphScriptVK::make_context(render_data, RendGraphContextVK{}));
    RendGraphDescVK graph = RendGraphRecipeBuilderVK().build(recipe, RendGraphRecipeImportsVK{}, &render_data, nullptr);
    ASSERT_EQ(graph.tasks.size(), 2U);
    EXPECT_EQ(graph.get_task("probe_1").next.size(), 1U);
    EXPECT_EQ(graph.samplers.size(), 1U);
}

TEST(WBERendGraphScriptVKTest, RecipesSizeDispatchesFromTheSwapChainExtent) {
    constexpr std::string_view TILED_RECIPE = R"(
return {
    build = function(context)
        local extent = context.swap_chain_extent
        return {
            tasks = {
                {
                    id = "tiled",
                    type = "run_compute_pipeline",
                    params = { pipeline = "tiles", group_count_x = (extent.width + 7) // 8, group_count_y = (extent.height + 7) // 8 },
                },
            },
        }
    end,
}
)";
    RenderAllocator allocator(WBE_KI_B(16));
    RenderContextVK render_data(&allocator);
    auto group_counts = [&render_data, TILED_RECIPE](uint32_t p_width, uint32_t p_height) {
        const RendGraphContextVK graph_context{.swap_chain_extent = {.width = p_width, .height = p_height}};
        const DataValue recipe =
            RendGraphScriptVK::run("tiled", TILED_RECIPE, RendGraphScriptVK::make_context(render_data, graph_context));
        const DataValue& params = recipe.get_entry("tasks").as_list().at(0).get_entry("params");
        return std::pair{params.get_value<uint32_t>("group_count_x"), params.get_value<uint32_t>("group_count_y")};
    };
    // A resize re-runs the recipe with the new extent, which rounds partial tiles up.
    EXPECT_EQ(group_counts(1920, 1080), std::pair(240U, 135U));
    EXPECT_EQ(group_counts(1921, 1080), std::pair(241U, 135U));
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
            global_allocator(), "assets/scripts/teal/lua_scripts.json", resource_manager->get_allocator()));
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

TEST_F(WBERendGraphScriptVKResourceTest, RunsTealScriptsCompiledByTheACP) {
    // tests.teal_recipe is a Teal module requiring another, which the ACP compiled to Lua.
    const DataValue recipe = RendGraphScriptVK::run("tests.teal_recipe", DataValue::make_map());
    EXPECT_EQ(recipe.get_entry("imports").as_list().at(0).as_string(), "6");
    EXPECT_EQ(recipe.get_value<std::string>("queue_family"), "compute_2");
}

TEST_F(WBERendGraphScriptVKResourceTest, SharedGraphDeclarationsLoadBeforeTheRuntimeAndTaskInstaller) {
    const DataValue recipe = RendGraphScriptVK::run("shared_graph_api",
        R"(
local common = require("render_graph_utils.render_graph")
local constructors = require("render_graph_utils.render_graph_tasks")
assert(type(constructors.install) == "function")
assert(common.new == nil)
local graph = require("render_graph_utils.render_graph_utils")
assert(graph == common)
assert(require("render_graph_utils.render_graph") == common)
assert(require("render_graph_utils.render_graph_tasks") == constructors)
return { build = function()
    local compute = graph.compute({ pipeline = graph.compute_pipeline("pipeline") }, { name = "compute" })
    return graph.build({ tasks = { compute } })
end }
)",
        DataValue::make_map());
    const auto& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 1U);
    EXPECT_EQ(tasks[0].get_value<std::string>("type"), "run_compute_pipeline");
    EXPECT_EQ(tasks[0].get_value<std::string>("name"), "compute");
    EXPECT_TRUE(is_generated_id(tasks[0].get_value<std::string>("id"), "run_compute_pipeline"));
    EXPECT_EQ(tasks[0].get_entry("params").get_value<std::string>("pipeline"), "pipeline");
    EXPECT_TRUE(recipe.get_entry("resources").as_list().empty());
    EXPECT_TRUE(recipe.get_entry("imports").as_list().empty());
}

TEST_F(WBERendGraphScriptVKResourceTest, CollectsHandleBindingsEdgesAndImageDependencies) {
    const DataValue recipe = RendGraphScriptVK::run("handle_resources",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local image = graph.storage_image({ name = "output_image", format = "r16g16b16a16_sfloat", extent = { width = 16, height = 8 } })
    local view = graph.image_view(image, { name = "output" })
    local sampler = graph.clamp_to_edge_sampler({ name = "sampler" })
    local scratch = graph.buffer({ name = "scratch", usage = "buffer_storage", size = 64 })
    local unused = graph.storage_buffer({ name = "unused", size = 32 })
    assert(graph.enum_name("image_read_write") == "IMAGE_READ_WRITE")
    local write = graph.task("registry_probe", { label = "write" }, { name = "write", bindings = { scratch = scratch } })
    local read = graph.task("registry_probe", { label = "read" }, { name = "read" })
    read:bind("input", view):bind("sampler", sampler)
    write:connect(read, view)
    return graph.build({ tasks = { write, read } })
end }
)",
        DataValue::make_map());
    const auto& resources = recipe.get_entry("resources").as_list();
    ASSERT_EQ(resources.size(), 4U);
    const std::string image = named_id(recipe, "output_image");
    const std::string view = named_id(recipe, "output");
    EXPECT_TRUE(is_generated_id(image, "image"));
    EXPECT_TRUE(is_generated_id(view, "image_view"));
    EXPECT_TRUE(is_generated_id(named_id(recipe, "sampler"), "sampler"));
    EXPECT_TRUE(is_generated_id(named_id(recipe, "scratch"), "buffer"));
    EXPECT_EQ(find_named_resource(recipe, "output").get_entry("image_view").get_value<std::string>("image"), image);
    EXPECT_THROW((void)find_named_resource(recipe, "unused"), std::runtime_error);
    const auto& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 2U);
    EXPECT_EQ(tasks[0].get_entry("connect").as_list()[0].get_value<std::string>("resource"), view);
    EXPECT_EQ(tasks[0].get_entry("connect").as_list()[0].get_value<std::string>("to"), named_task_id(recipe, "read"));
    EXPECT_EQ(tasks[0].get_entry("aliases").get_value<std::string>("scratch"), named_id(recipe, "scratch"));
    EXPECT_EQ(tasks[1].get_entry("aliases").get_value<std::string>("input"), view);
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_context(&allocator);
    auto graph = RendGraphRecipeBuilderVK().build(recipe, RendGraphRecipeImportsVK{}, &render_context, nullptr);
    EXPECT_EQ(graph.images.size(), 1U);
    EXPECT_EQ(graph.image_views.size(), 1U);
    EXPECT_EQ(graph.samplers.size(), 1U);
    EXPECT_EQ(graph.buffers.size(), 1U);
    // The debug names reach the descriptor.
    EXPECT_EQ(graph.debug_names.find(view)->second, "output");
    EXPECT_EQ(graph.get_task(named_task_id(recipe, "write")).debug_name, "write");
}

TEST_F(WBERendGraphScriptVKResourceTest, GeneratedIdsAreDeterministicAndNamedIdsIgnoreOtherDeclarations) {
    constexpr std::string_view RECIPE = R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function(context)
    local tasks = {}
    if context.extra then
        tasks[#tasks + 1] = graph.task("registry_probe", { label = "extra" })
    end
    local task = graph.task("registry_probe", { label = "probe" }, { name = "probe" })
    tasks[#tasks + 1] = task
    tasks[#tasks + 1] = graph.task("registry_probe", { label = "unnamed" })
    if context.extra then
        task:bind("extra", graph.storage_buffer({ size = 8 }))
    end
    task:bind("unnamed", graph.storage_buffer({ size = 16 }))
    task:bind("first", graph.storage_buffer({ name = "shared", size = 16 }))
    task:bind("second", graph.storage_buffer({ name = "shared", size = 32 }))
    task:bind("named", graph.storage_buffer({ name = "named", size = 64 }))
    return graph.build({ tasks = tasks })
end }
)";
    DataValue context = DataValue::make_map();
    context.set_value("extra", false);
    const DataValue first = RendGraphScriptVK::run("generated_ids", RECIPE, context);
    const DataValue second = RendGraphScriptVK::run("generated_ids", RECIPE, context);
    EXPECT_EQ(first.get_entry("resources"), second.get_entry("resources"));
    EXPECT_EQ(first.get_entry("tasks"), second.get_entry("tasks"));
    context.set_value("extra", true);
    const DataValue extended = RendGraphScriptVK::run("generated_ids", RECIPE, context);
    // An unnamed resource or task declared before them shifts no named one's id; ones sharing a name differ by order.
    EXPECT_EQ(named_id(extended, "named"), named_id(first, "named"));
    EXPECT_EQ(named_task_id(extended, "probe"), named_task_id(first, "probe"));
    EXPECT_TRUE(is_generated_id(named_task_id(first, "probe"), "registry_probe"));
    const DataValue& aliases = find_named_task(first, "probe").get_entry("aliases");
    const DataValue& extended_aliases = find_named_task(extended, "probe").get_entry("aliases");
    EXPECT_EQ(extended_aliases.get_value<std::string>("first"), aliases.get_value<std::string>("first"));
    EXPECT_EQ(extended_aliases.get_value<std::string>("second"), aliases.get_value<std::string>("second"));
    EXPECT_NE(aliases.get_value<std::string>("first"), aliases.get_value<std::string>("second"));
    EXPECT_NE(extended_aliases.get_value<std::string>("unnamed"), aliases.get_value<std::string>("unnamed"));
    // The unnamed task after the named one is the second unnamed probe of the extended recipe, so its id shifts.
    EXPECT_NE(extended.get_entry("tasks").as_list()[2].get_value<std::string>("id"),
        first.get_entry("tasks").as_list()[1].get_value<std::string>("id"));
    for (const DataValue& resource : extended.get_entry("resources").as_list()) {
        EXPECT_TRUE(is_generated_id(resource.get_value<std::string>("id"), "buffer"));
    }
}

TEST_F(WBERendGraphScriptVKResourceTest, BuildsOnlyTheListedTasksOnce) {
    const auto build_error = [](std::string_view p_script) {
        try {
            (void)RendGraphScriptVK::run("listed_tasks", p_script, DataValue::make_map());
        } catch (const std::runtime_error& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    // An edge to a task outside the build fails.
    EXPECT_NE(build_error(R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local buffer = graph.storage_buffer({ size = 16 })
    local first = graph.task("registry_probe", { label = "first" }, { bindings = { buffer = buffer } })
    local second = graph.task("registry_probe", { label = "second" }, { bindings = { buffer = buffer } })
    first:connect(second, buffer)
    return graph.build({ tasks = { first } })
end }
)")
                  .find("an edge leaves the built tasks"),
        std::string::npos);
    // A task listed twice fails.
    EXPECT_NE(build_error(R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local task = graph.task("registry_probe", { label = "task" })
    return graph.build({ tasks = { task, task } })
end }
)")
                  .find("is listed twice"),
        std::string::npos);
    // A task that is not listed is not built, and neither are the resources only it reaches.
    const DataValue recipe = RendGraphScriptVK::run("unlisted_tasks",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local listed = graph.task("registry_probe", { label = "listed" })
    graph.task("registry_probe", { label = "unlisted" }, { bindings = { buffer = graph.storage_buffer({ size = 16 }) } })
    return graph.build({ tasks = { listed } })
end }
)",
        DataValue::make_map());
    EXPECT_EQ(recipe.get_entry("tasks").as_list().size(), 1U);
    EXPECT_TRUE(recipe.get_entry("resources").as_list().empty());
}

TEST_F(WBERendGraphScriptVKResourceTest, ExportsNameResourcesForCAndRejectDuplicates) {
    const DataValue recipe = RendGraphScriptVK::run("exports",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local image = graph.image({ name = "output", usage = "image_read_write", format = "r16g16b16a16_sfloat" })
    local unused = graph.storage_buffer({ size = 16 })
    assert(not pcall(function() graph.build({ tasks = {}, exports = { mesh = graph.mesh("mesh") } }) end))
    assert(not pcall(function() graph.build({ tasks = {}, exports = { [""] = unused } }) end))
    return graph.build({ tasks = {}, exports = { output_image = image, scratch = unused } })
end }
)",
        DataValue::make_map());
    const DataValue& exports = recipe.get_entry("exports");
    EXPECT_EQ(exports.get_value<std::string>("output_image"), named_id(recipe, "output"));
    // An exported resource is declared even though no task uses it.
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 2U);
    EXPECT_TRUE(is_generated_id(exports.get_value<std::string>("scratch"), "buffer"));
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_context(&allocator);
    const auto graph = RendGraphRecipeBuilderVK().build(recipe, RendGraphRecipeImportsVK{}, &render_context, nullptr);
    EXPECT_EQ(graph.exports.find("output_image")->second, named_id(recipe, "output"));
}

TEST_F(WBERendGraphScriptVKResourceTest, SubgraphsBuildNestedRecipesWithTheirPorts) {
    const DataValue recipe = RendGraphScriptVK::run("handle_ports",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local target_image = graph.storage_image({ name = "target_image", format = "r16g16b16a16_sfloat", extent = { width = 16, height = 8 } })
    local target = graph.image_view(target_image, { name = "target" })
    local input = graph.import("input")
    local member = input:member("member")
    assert(not pcall(function() member:member("nested") end))
    assert(not pcall(function() target:member("member") end))
    local color = graph.port("color")
    local scratch = graph.storage_buffer({ name = "scratch", size = 16 })
    local draw = graph.task("registry_probe", { label = "draw" }, { name = "draw", bindings = { output = color, scratch = scratch } })
    local resolve = graph.task("registry_probe", { label = "resolve" }, { bindings = { scratch = scratch } })
    draw:connect(resolve, scratch)
    local prepare = graph.task("registry_probe", { label = "prepare" }, { name = "prepare", bindings = { input = input, member = member } })
    local effect = graph.subgraph({
        name = "effect",
        tasks = { draw, resolve },
        ports = { unused = target, color = target },
        toggle = { event = "effect_toggle", enabled = true },
    })
    graph.chain(target, { prepare, effect })
    return graph.build({ tasks = { prepare, effect } })
end }
)",
        DataValue::make_map());
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 2U);
    const auto& imports = recipe.get_entry("imports").as_list();
    ASSERT_EQ(imports.size(), 1U);
    EXPECT_EQ(imports[0].as_string(), "input");
    const DataValue& prepare = find_named_task(recipe, "prepare");
    EXPECT_EQ(prepare.get_entry("aliases").get_value<std::string>("input"), "input");
    EXPECT_EQ(prepare.get_entry("aliases").get_value<std::string>("member"), "input.member");
    const DataValue& effect = find_named_task(recipe, "effect");
    EXPECT_TRUE(is_generated_id(effect.get_value<std::string>("id"), "subgraph"));
    EXPECT_EQ(prepare.get_entry("connect").as_list()[0].get_value<std::string>("to"), effect.get_value<std::string>("id"));
    EXPECT_TRUE(effect.get_entry("toggle").get_value<bool>("enabled"));
    const std::string target = named_id(recipe, "target");
    EXPECT_EQ(effect.get_entry("ports").get_value<std::string>("color"), target);
    EXPECT_EQ(effect.get_entry("ports").get_value<std::string>("unused"), target);
    // The nested recipe declares its own resources and tasks, with ids of its own, and every port, used or not.
    const auto& nested = effect.get_entry("subgraph");
    EXPECT_EQ(nested.get_entry("resources").as_list().size(), 1U);
    EXPECT_TRUE(nested.get_entry("imports").as_list().empty());
    ASSERT_EQ(nested.get_entry("ports").as_list().size(), 2U);
    EXPECT_EQ(nested.get_entry("ports").as_list()[0].as_string(), "color");
    EXPECT_EQ(nested.get_entry("ports").as_list()[1].as_string(), "unused");
    const DataValue& draw = find_named_task(nested, "draw");
    EXPECT_EQ(draw.get_entry("aliases").get_value<std::string>("output"), "color");
    EXPECT_EQ(draw.get_entry("connect").as_list()[0].get_value<std::string>("resource"), named_id(nested, "scratch"));
}

TEST_F(WBERendGraphScriptVKResourceTest, RejectsCyclicHandleDependencies) {
    EXPECT_THROW((void)RendGraphScriptVK::run("cyclic_handles",
                     R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local resource = graph.storage_buffer({ size = 16 })
    resource.dependencies = { resource }
    return graph.build({ tasks = { graph.task("registry_probe", nil, { bindings = { input = resource } }) } })
end }
)",
                     DataValue::make_map()),
        std::runtime_error);
}

TEST_F(WBERendGraphScriptVKResourceTest, ReusesHandlesWithoutLeakingDeclarationsBetweenBuilds) {
    const DataValue result = RendGraphScriptVK::run("independent_graphs",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local shared = graph.storage_buffer({ name = "shared", size = 16 })
    local first = graph.task("registry_probe", nil, { bindings = { input = shared, again = shared } })
    local second = graph.task("registry_probe", nil)
    local third = graph.task("registry_probe", nil, { bindings = { input = shared } })
    return {
        first = graph.build({ tasks = { first } }),
        second = graph.build({ tasks = { second } }),
        third = graph.build({ tasks = { third } }),
    }
end }
)",
        DataValue::make_map());
    EXPECT_EQ(result.get_entry("first").get_entry("resources").as_list().size(), 1U);
    EXPECT_TRUE(result.get_entry("second").get_entry("resources").as_list().empty());
    EXPECT_EQ(result.get_entry("third").get_entry("resources").as_list().size(), 1U);
    // Every build generates its own ids, in the same way.
    EXPECT_EQ(named_id(result.get_entry("first"), "shared"), named_id(result.get_entry("third"), "shared"));
}

TEST_F(WBERendGraphScriptVKResourceTest, TaskAliasesResolveIndependentlyAndConnectionsLowerHandles) {
    const DataValue recipe = RendGraphScriptVK::run("task_handles",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local first_resource = graph.storage_buffer({ name = "first_resource", size = 16 })
    local second_resource = graph.storage_buffer({ name = "second_resource", size = 32 })
    local first = graph.task("registry_probe", nil, { name = "first" })
    local second = graph.task("registry_probe", nil, { name = "second" })
    assert(first:bind("input", first_resource) == first)
    assert(second:bind("input", second_resource) == second)
    assert(first:connect(second, first_resource) == first)
    return graph.build({ tasks = { first, second } })
end }
)",
        DataValue::make_map());
    const auto& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 2U);
    EXPECT_EQ(tasks[0].get_entry("aliases").get_value<std::string>("input"), named_id(recipe, "first_resource"));
    EXPECT_EQ(tasks[1].get_entry("aliases").get_value<std::string>("input"), named_id(recipe, "second_resource"));
    const auto& edge = tasks[0].get_entry("connect").as_list()[0];
    EXPECT_EQ(edge.get_value<std::string>("to"), named_task_id(recipe, "second"));
    EXPECT_EQ(edge.get_value<std::string>("resource"), named_id(recipe, "first_resource"));
    for (const auto& task : tasks) {
        EXPECT_FALSE(task.contains("alias"));
        EXPECT_FALSE(task.contains("bindings"));
        EXPECT_FALSE(task.contains("edges"));
        EXPECT_FALSE(task.contains("kind"));
    }
}

TEST_F(WBERendGraphScriptVKResourceTest, TaskAliasMapsAreOwnedByEachTask) {
    const DataValue recipe = RendGraphScriptVK::run("independent_aliases",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local shared = graph.storage_buffer({ size = 16 })
    local extra = graph.storage_buffer({ size = 32 })
    local bindings = { input = shared }
    local first = graph.task("registry_probe", nil, { bindings = bindings })
    local second = graph.task("registry_probe", nil, { bindings = bindings })
    first:bind("extra", extra)
    assert(bindings.extra == nil)
    return graph.build({ tasks = { first, second } })
end }
)",
        DataValue::make_map());
    const auto& tasks = recipe.get_entry("tasks").as_list();
    EXPECT_TRUE(tasks[0].get_entry("aliases").contains("extra"));
    EXPECT_FALSE(tasks[1].get_entry("aliases").contains("extra"));
}

TEST_F(WBERendGraphScriptVKResourceTest, BatchedConnectionsLowerEachResourceAndPreserveSingleResourceCalls) {
    const DataValue recipe = RendGraphScriptVK::run("batched_connections",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
local function compute(key)
    return graph.compute({ pipeline = graph.compute_pipeline(key) }, { name = key })
end
return { build = function()
    local color = graph.storage_buffer({ name = "color", size = 16 })
    local depth = graph.storage_buffer({ name = "depth", size = 32 })
    local first = compute("first")
    local second = compute("second")
    local third = compute("third")
    local last = compute("last")
    local sink = compute("sink")
    graph.chain({ color, depth }, { first, second, third })
    assert(third:connect(last, { color, depth }) == third)
    graph.chain(color, {})
    graph.chain(depth, { last })
    last:connect(sink, color)
    return graph.build({ tasks = { first, second, third, last, sink } })
end }
)",
        DataValue::make_map());
    const std::string color = named_id(recipe, "color");
    const std::string depth = named_id(recipe, "depth");
    const auto& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 5U);
    for (size_t i = 0; i < 3; ++i) {
        const auto& edges = tasks[i].get_entry("connect").as_list();
        ASSERT_EQ(edges.size(), 2U);
        EXPECT_EQ(edges[0].get_value<std::string>("to"), tasks[i + 1].get_value<std::string>("id"));
        EXPECT_EQ(edges[1].get_value<std::string>("to"), tasks[i + 1].get_value<std::string>("id"));
        EXPECT_EQ(edges[0].get_value<std::string>("resource"), color);
        EXPECT_EQ(edges[1].get_value<std::string>("resource"), depth);
    }
    EXPECT_EQ(tasks[3].get_entry("connect").as_list().size(), 1U);
    EXPECT_EQ(tasks[3].get_entry("connect").as_list()[0].get_value<std::string>("resource"), color);
    EXPECT_FALSE(tasks[4].contains("connect"));
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 2U);
}

TEST_F(WBERendGraphScriptVKResourceTest, ConnectionFanOutLeavesConsumersUnordered) {
    const DataValue recipe = RendGraphScriptVK::run("connection_fan_out",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
local function compute(key)
    return graph.compute({ pipeline = graph.compute_pipeline(key) }, { name = key })
end
return { build = function()
    local color = graph.storage_buffer({ name = "color", size = 16 })
    local depth = graph.storage_buffer({ name = "depth", size = 32 })
    local producer = compute("producer")
    local first = compute("first")
    local second = compute("second")
    assert(producer:connect_all({ first, second }, { color, depth }) == producer)
    return graph.build({ tasks = { producer, first, second } })
end }
)",
        DataValue::make_map());
    const std::string color = named_id(recipe, "color");
    const std::string depth = named_id(recipe, "depth");
    const std::string first = named_task_id(recipe, "first");
    const std::string second = named_task_id(recipe, "second");
    const auto& tasks = recipe.get_entry("tasks").as_list();
    const auto& edges = tasks[0].get_entry("connect").as_list();
    ASSERT_EQ(edges.size(), 4U);
    EXPECT_EQ(edges[0].get_value<std::string>("to"), first);
    EXPECT_EQ(edges[0].get_value<std::string>("resource"), color);
    EXPECT_EQ(edges[1].get_value<std::string>("to"), second);
    EXPECT_EQ(edges[1].get_value<std::string>("resource"), color);
    EXPECT_EQ(edges[2].get_value<std::string>("to"), first);
    EXPECT_EQ(edges[2].get_value<std::string>("resource"), depth);
    EXPECT_EQ(edges[3].get_value<std::string>("to"), second);
    EXPECT_EQ(edges[3].get_value<std::string>("resource"), depth);
    EXPECT_FALSE(tasks[1].contains("connect"));
    EXPECT_FALSE(tasks[2].contains("connect"));
}

TEST_F(WBERendGraphScriptVKResourceTest, InputsFromBindsTaskLocalNamesAndConnectsEachHandleOnce) {
    const DataValue recipe = RendGraphScriptVK::run("input_connections",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
local function compute(key)
    return graph.compute({ pipeline = graph.compute_pipeline(key) }, { name = key })
end
return { build = function()
    local color = graph.storage_buffer({ name = "color", size = 16 })
    local normal = graph.storage_buffer({ name = "normal", size = 32 })
    local other = graph.storage_buffer({ name = "other", size = 64 })
    local producer = compute("producer")
    local first = compute("first")
    local second = compute("second")
    assert(first:inputs_from(producer, { normal = normal, albedo = color, albedo_again = color }) == first)
    second:inputs_from(producer, { albedo = other })
    return graph.build({ tasks = { producer, first, second } })
end }
)",
        DataValue::make_map());
    const std::string color = named_id(recipe, "color");
    const std::string normal = named_id(recipe, "normal");
    const std::string other = named_id(recipe, "other");
    const auto& tasks = recipe.get_entry("tasks").as_list();
    const auto& edges = tasks[0].get_entry("connect").as_list();
    ASSERT_EQ(edges.size(), 3U);
    // Each task's inputs connect in the order of their sorted task-local names.
    EXPECT_EQ(edges[0].get_value<std::string>("to"), named_task_id(recipe, "first"));
    EXPECT_EQ(edges[0].get_value<std::string>("resource"), color);
    EXPECT_EQ(edges[1].get_value<std::string>("to"), named_task_id(recipe, "first"));
    EXPECT_EQ(edges[1].get_value<std::string>("resource"), normal);
    EXPECT_EQ(edges[2].get_value<std::string>("to"), named_task_id(recipe, "second"));
    EXPECT_EQ(edges[2].get_value<std::string>("resource"), other);
    const auto& first_aliases = tasks[1].get_entry("aliases");
    EXPECT_EQ(first_aliases.get_value<std::string>("albedo"), color);
    EXPECT_EQ(first_aliases.get_value<std::string>("albedo_again"), color);
    EXPECT_EQ(first_aliases.get_value<std::string>("normal"), normal);
    EXPECT_EQ(tasks[2].get_entry("aliases").get_value<std::string>("albedo"), other);
    EXPECT_FALSE(tasks[1].contains("connect"));
    EXPECT_FALSE(tasks[2].contains("connect"));
    EXPECT_FALSE(tasks[1].contains("inputs_from"));
    EXPECT_FALSE(tasks[0].contains("connect_all"));
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 3U);
}

TEST_F(WBERendGraphScriptVKResourceTest, ConnectionBatchesValidateBeforeChangingTheTasks) {
    const DataValue recipe = RendGraphScriptVK::run("connection_batch_validation",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local resource = graph.storage_buffer({ size = 16 })
    local other = graph.storage_buffer({ size = 32 })
    local producer = graph.compute({ pipeline = graph.compute_pipeline("producer") })
    local consumer = graph.compute_indirect({ pipeline = graph.compute_pipeline("consumer"), indirect_buffer = resource })
    consumer:bind("z_existing", other)
    local forged = { kind = "typed", edges = {}, bindings = {} }
    local invalid_calls = {
        function() producer:connect_all({ consumer, forged }, resource) end,
        function() producer:connect(consumer, { resource, "invalid" }) end,
        function() graph.chain({ resource, "invalid" }, { producer, consumer }) end,
        function() graph.chain(resource, { producer, consumer, forged }) end,
        function() consumer:inputs_from(forged, { a_input = resource }) end,
        function() consumer:inputs_from(producer, { a_input = resource, z_existing = other }) end,
        function() consumer:inputs_from(producer, { a_input = resource, z_invalid = "invalid" }) end,
        function() producer:connect(consumer, {}) end,
        function() producer:connect_all({}, resource) end,
        function() consumer:inputs_from(producer, {}) end,
        function() producer:connect(consumer, { [1] = resource, [3] = other }) end,
        function() producer:connect_all({ [1] = consumer, [3] = consumer }, resource) end,
        function() graph.chain(resource, { [1] = producer, [3] = consumer }) end,
        function() producer:connect(consumer, { resource, extra = other }) end,
        function() producer:connect(consumer, { kind = "buffer", dependencies = {} }) end,
    }
    for _, call in ipairs(invalid_calls) do
        local succeeded = pcall(call)
        assert(not succeeded)
        assert(#producer.edges == 0)
        assert(consumer.bindings.a_input == nil)
        assert(consumer.bindings.z_existing == other)
    end
    return graph.build({ tasks = { producer, consumer } })
end }
)",
        DataValue::make_map());
    const auto& tasks = recipe.get_entry("tasks").as_list();
    EXPECT_FALSE(tasks[0].contains("connect"));
    EXPECT_FALSE(tasks[1].contains("connect"));
    EXPECT_FALSE(tasks[1].get_entry("aliases").contains("a_input"));
}

TEST_F(WBERendGraphScriptVKResourceTest, RejectsTablesThatAreNotHandles) {
    // A table shaped like a task or resource handle is not one.
    EXPECT_THROW((void)RendGraphScriptVK::run("forged_tasks",
                     R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    return graph.build({ tasks = { { kind = "typed", ["type"] = "registry_probe", edges = {}, bindings = {} } } })
end }
)",
                     DataValue::make_map()),
        std::runtime_error);
    EXPECT_THROW((void)RendGraphScriptVK::run("forged_resources",
                     R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    return graph.build({ tasks = { graph.task("registry_probe", nil, { bindings = { input = { kind = "buffer", dependencies = {} } } }) } })
end }
)",
                     DataValue::make_map()),
        std::runtime_error);
}

TEST_F(WBERendGraphScriptVKResourceTest, RejectsRepeatedTaskLocalAliasNames) {
    EXPECT_THROW((void)RendGraphScriptVK::run("repeated_alias",
                     R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local resource = graph.storage_buffer({ size = 16 })
    local task = graph.task("registry_probe", nil, { bindings = { input = resource } })
    task:bind("input", resource)
    return graph.build({ tasks = { task } })
end }
)",
                     DataValue::make_map()),
        std::runtime_error);
}

TEST_F(WBERendGraphScriptVKResourceTest, MultiplexersTakePlainBranchesAndRejectThoseTheyCannotCombine) {
    const auto run = [](std::string_view p_body) {
        return RendGraphScriptVK::run("multiplex_error",
            std::format(R"(
local graph = require("render_graph_utils.render_graph_utils")
return {{ build = function()
    local first = graph.storage_buffer({{ size = 16 }})
    local second = graph.storage_buffer({{ size = 16 }})
    local function probe(options)
        return graph.task("registry_probe", nil, options)
    end
    return graph.build({{ tasks = {{ {} }} }})
end }}
)",
                p_body),
            DataValue::make_map());
    };
    // Branches share one resource map, so a task-local id cannot name two resources.
    EXPECT_THROW((void)run(R"(graph.multiplex({ branches = {
        probe({ bindings = { input = first } }),
        probe({ bindings = { input = second } }),
    }, select = { event = "e" } }))"),
        std::runtime_error);
    // Ordering and gating belong to the multiplexer, not to a branch.
    EXPECT_THROW((void)run(R"(graph.multiplex({ branches = { probe({ toggle = { event = "t" } }) }, select = { event = "e" } }))"),
        std::runtime_error);
    EXPECT_THROW((void)run(R"(graph.multiplex({ branches = { graph.import_task("imported") }, select = { event = "e" } }))"),
        std::runtime_error);
    EXPECT_THROW((void)run(R"(graph.multiplex({ branches = { probe() } }))"), std::runtime_error);
    EXPECT_THROW((void)run(R"(graph.multiplex({ branches = {}, select = { event = "e" } }))"), std::runtime_error);
    const DataValue recipe = run(R"(graph.multiplex({ name = "m", branches = {
        probe({ bindings = { input = first } }),
        probe({ bindings = { input = first } }),
    }, select = { event = "e", branch = 1 } }))");
    // The branches are not tasks of the recipe themselves; they become the multiplexer's branches.
    const auto& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 1U);
    EXPECT_TRUE(is_generated_id(tasks[0].get_value<std::string>("id"), "multiplex"));
    EXPECT_EQ(tasks[0].get_value<std::string>("name"), "m");
    EXPECT_EQ(tasks[0].get_entry("branches").as_list().size(), 2U);
    EXPECT_EQ(tasks[0].get_entry("select").get_value<uint32_t>("branch"), 1U);
    EXPECT_EQ(tasks[0].get_entry("aliases").get_value<std::string>("input"),
        recipe.get_entry("resources").as_list()[0].get_value<std::string>("id"));
}

TEST_F(WBERendGraphScriptVKResourceTest, TypedConstructorsLowerResourceParametersAndKeepPlainRecipeTables) {
    const DataValue recipe = RendGraphScriptVK::run("tests.teal_graph_api", DataValue::make_map());
    const auto& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 12U);
    const std::string color = named_id(recipe, "color_image", "image_view");
    const std::string mips = named_id(recipe, "color_mips");
    const std::string commands = named_id(recipe, "commands");
    EXPECT_EQ(tasks[0].get_value<std::string>("type"), "run_compute_pipeline");
    EXPECT_EQ(tasks[0].get_value<std::string>("name"), "prepare");
    EXPECT_EQ(tasks[0].get_entry("params").get_value<std::string>("pipeline"), "prepare_pipeline");
    EXPECT_TRUE(tasks[0].get_entry("trigger").get_value<bool>("activate"));
    EXPECT_TRUE(tasks[0].get_entry("toggle").get_value<bool>("enabled"));
    EXPECT_EQ(tasks[1].get_value<std::string>("type"), "run_graphics_pipeline");
    EXPECT_EQ(tasks[1].get_entry("aliases").get_value<std::string>("render_target_colors[0]"), color);
    EXPECT_EQ(tasks[2].get_value<std::string>("type"), "graphics_draw_mesh");
    EXPECT_FALSE(tasks[2].contains("name"));
    EXPECT_EQ(tasks[2].get_entry("params").get_value<std::string>("transform"), commands);
    EXPECT_EQ(tasks[2].get_entry("params").get_value<std::string>("mesh"), "mesh_key");
    EXPECT_EQ(tasks[3].get_value<std::string>("type"), "run_graphics_pipeline_indirect");
    EXPECT_EQ(tasks[3].get_entry("params").get_value<std::string>("index_buffer"), commands);
    EXPECT_EQ(tasks[3].get_entry("params").get_value<std::string>("indirect_buffer"), commands);
    EXPECT_EQ(tasks[4].get_value<std::string>("type"), "run_compute_pipeline_indirect");
    EXPECT_EQ(tasks[4].get_entry("params").get_value<std::string>("indirect_buffer"), commands);
    EXPECT_EQ(tasks[5].get_value<std::string>("type"), "run_compute_pipelines_sequential");
    const auto& dispatches = tasks[5].get_entry("params").get_entry("dispatches").as_list();
    ASSERT_EQ(dispatches.size(), 3U);
    // A handle lowers to its graph id, which the task binds to itself.
    const DataValue& sequence_aliases = tasks[5].get_entry("aliases");
    EXPECT_EQ(dispatches[1].get_value<std::string>("indirect_buffer"), commands);
    EXPECT_EQ(dispatches[1].get_entry("bindings").get_value<std::string>("shader_buffer"), commands);
    EXPECT_EQ(dispatches[2].get_value<std::string>("indirect_buffer"), commands);
    EXPECT_EQ(sequence_aliases.get_value<std::string>(commands), commands);
    EXPECT_EQ(tasks[6].get_value<std::string>("type"), "generate_mipmaps");
    EXPECT_EQ(tasks[6].get_entry("params").get_value<std::string>("source_view"), color);
    EXPECT_EQ(tasks[6].get_entry("params").get_value<std::string>("mip_chain_view"), mips);
    EXPECT_EQ(tasks[7].get_value<std::string>("type"), "cpu_acquire_image");
    EXPECT_EQ(tasks[7].get_entry("params").get_value<std::string>("image_view"), color);
    EXPECT_EQ(tasks[7].get_entry("params").get_value<std::string>("event"), "readback_event");
    EXPECT_EQ(tasks[8].get_value<std::string>("type"), "full_memory_barrier");
    EXPECT_EQ(tasks[8].get_entry("params").get_value<std::string>("resource"), commands);
    EXPECT_EQ(tasks[9].get_value<std::string>("type"), "ping_pong");
    // The multiplexer sets no type or params of its own; its branches carry them, and their aliases are merged.
    EXPECT_FALSE(tasks[10].contains("type"));
    EXPECT_FALSE(tasks[10].contains("params"));
    EXPECT_TRUE(is_generated_id(tasks[10].get_value<std::string>("id"), "multiplex"));
    const auto& branches = tasks[10].get_entry("branches").as_list();
    ASSERT_EQ(branches.size(), 2U);
    EXPECT_EQ(branches[0].get_value<std::string>("type"), "run_compute_pipeline");
    EXPECT_EQ(branches[0].get_entry("params").get_value<int64_t>("group_count_x"), 4);
    EXPECT_EQ(branches[1].get_entry("params").get_value<std::string>("pipeline"), "indirect_pipeline");
    EXPECT_EQ(tasks[10].get_entry("select").get_value<std::string>("event"), "select_event");
    EXPECT_EQ(tasks[10].get_entry("select").get_value<int64_t>("branch"), 1);
    EXPECT_TRUE(tasks[10].get_entry("toggle").get_value<bool>("enabled"));
    EXPECT_EQ(tasks[10].get_entry("aliases").get_value<std::string>("shader_input"), "input");
    // An imported task keeps its id.
    EXPECT_EQ(tasks[11].get_value<std::string>("id"), "overlay");
    EXPECT_TRUE(tasks[11].get_value<bool>("import"));
    EXPECT_FALSE(tasks[11].contains("params"));
    EXPECT_FALSE(tasks[11].contains("type"));
    for (const auto& task : tasks) {
        EXPECT_FALSE(task.contains("uses"));
        EXPECT_FALSE(task.contains("graph"));
        EXPECT_FALSE(task.contains("color_target"));
        EXPECT_FALSE(task.contains("depth_target"));
    }
    // Pipeline and mesh keys and event strings never become resource imports. Parameter handles emit one declaration each.
    ASSERT_EQ(recipe.get_entry("imports").as_list().size(), 1U);
    EXPECT_EQ(recipe.get_entry("imports").as_list()[0].as_string(), "input");
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 4U);
    EXPECT_EQ(recipe.get_entry("exports").get_value<std::string>("color"), color);
    EXPECT_FALSE(tasks[2].contains("connect"));
    EXPECT_FALSE(tasks[5].contains("connect"));
}

TEST_F(WBERendGraphScriptVKResourceTest, RegisteredResourcesLowerWithoutGraphResourcesAndSnapshotTheirValues) {
    const DataValue recipe = RendGraphScriptVK::run("registered_keys",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local mesh = graph.mesh("static_mesh")
    local graphics = graph.graphics_pipeline("shared_name")
    local compute = graph.compute_pipeline("shared_name")
    graph.mesh("unused_mesh")
    local draw = graph.draw_mesh({ mesh = mesh, pipeline = { default = graphics }, parts = { "opaque" } })
    local tasks = {
        draw,
        graph.compute({ pipeline = compute }),
        graph.compute_sequential({ dispatches = { { pipeline = compute } } }),
        graph.ping_pong({ ping_pipeline = compute, pong_pipeline = compute, iterations = 2,
            ping_prefix_pipeline = compute, pong_prefix_pipeline = compute }),
        graph.ping_pong({ ping_pipeline = compute, pong_pipeline = compute, iterations = 1 }),
    }
    mesh.key = "changed_mesh"
    graphics.key = "changed_graphics"
    -- A registered key does not reserve a task-local binding with the same name.
    draw:bind("static_mesh", graph.storage_buffer({ name = "buffer", size = 16 }))
    return graph.build({ tasks = tasks })
end }
)",
        DataValue::make_map());
    const auto& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 5U);
    // Keys are read when the recipe is built.
    EXPECT_EQ(tasks[0].get_entry("params").get_value<std::string>("mesh"), "changed_mesh");
    EXPECT_EQ(tasks[0].get_entry("params").get_entry("pipeline").get_value<std::string>("default"), "changed_graphics");
    EXPECT_EQ(tasks[1].get_entry("params").get_value<std::string>("pipeline"), "shared_name");
    const auto& dispatch = tasks[2].get_entry("params").get_entry("dispatches").as_list()[0];
    EXPECT_EQ(dispatch.get_value<std::string>("pipeline"), "shared_name");
    for (const char* field : {"ping_pipeline", "pong_pipeline", "ping_prefix_pipeline", "pong_prefix_pipeline"}) {
        EXPECT_EQ(tasks[3].get_entry("params").get_value<std::string>(field), "shared_name");
    }
    EXPECT_FALSE(tasks[4].get_entry("params").contains("ping_prefix_pipeline"));
    EXPECT_FALSE(tasks[4].get_entry("params").contains("pong_prefix_pipeline"));
    for (const auto& task : tasks) {
        EXPECT_FALSE(task.contains("connect"));
    }
    EXPECT_FALSE(tasks[1].contains("aliases"));
    EXPECT_FALSE(tasks[2].contains("aliases"));
    EXPECT_EQ(tasks[0].get_entry("aliases").get_value<std::string>("static_mesh"), named_id(recipe, "buffer"));
    EXPECT_TRUE(recipe.get_entry("imports").as_list().empty());
    EXPECT_TRUE(recipe.get_entry("registrations").as_list().empty());
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 1U);
}

TEST_F(WBERendGraphScriptVKResourceTest, RegistrationsAreEmittedWhenTasksReachTheirHandles) {
    const DataValue recipe = RendGraphScriptVK::run("registrations",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local params = { pipeline = "source_pipeline", color_formats = { "r8g8b8a8_unorm" } }
    local graphics = graph.graphics_pipeline("graphics", params)
    params.pipeline = "changed"
    local compute = graph.compute_pipeline("compute", { shader = "compute_shader" })
    local mesh = graph.mesh("mesh", { mesh = "mesh_resource" })
    local image = graph.registered_image("image", { texture = "texture_resource" })
    local custom = graph.registered_resource("custom", "CustomParams", { value = 3 }, "WhiteBirdEngine::ROComputePipelineVK")
    graph.mesh("unused", { mesh = "unused_resource" })
    local draw = graph.draw_mesh({ mesh = mesh, pipeline = { default = graphics }, parts = { "opaque" } })
    local dispatch = graph.compute({ pipeline = compute }):bind("input", image)
    local other = graph.compute({ pipeline = custom })
    -- Equal registrations of a key merge; a different one fails when both are built.
    local same = graph.compute({ pipeline = graph.compute_pipeline("compute", { shader = "compute_shader" }) })
    local different = graph.compute({ pipeline = graph.compute_pipeline("compute", { shader = "other_shader" }) })
    assert(not pcall(function() graph.build({ tasks = { dispatch, different } }) end))
    return graph.build({ tasks = { draw, dispatch, other, same } })
end }
)",
        DataValue::make_map());
    const auto& registrations = recipe.get_entry("registrations").as_list();
    ASSERT_EQ(registrations.size(), 5U);
    // In the order the build reaches them; an unreached registration is not emitted.
    const std::vector<std::pair<std::string, std::string>> expected{{"mesh", "RDParamsMeshVK"},
        {"graphics", "RDParamsGraphicsPipelineVK"},
        {"compute", "RDParamsComputePipelineVK"},
        {"image", "RDParamsImageVK"},
        {"custom", "CustomParams"}};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(registrations[i].get_value<std::string>("key"), expected[i].first);
        EXPECT_EQ(registrations[i].get_value<std::string>("type"), expected[i].second);
    }
    // Descriptions are snapshots.
    EXPECT_EQ(registrations[1].get_entry("description").get_value<std::string>("pipeline"), "source_pipeline");
    EXPECT_EQ(registrations[4].get_entry("description").get_value<int64_t>("value"), 3);
    const auto& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 4U);
    EXPECT_EQ(tasks[0].get_entry("params").get_value<std::string>("mesh"), "mesh");
    EXPECT_EQ(tasks[0].get_entry("params").get_entry("pipeline").get_value<std::string>("default"), "graphics");
    EXPECT_EQ(tasks[1].get_entry("params").get_value<std::string>("pipeline"), "compute");
    // A registered image is bound like an import.
    ASSERT_EQ(recipe.get_entry("imports").as_list().size(), 1U);
    EXPECT_EQ(recipe.get_entry("imports").as_list()[0].as_string(), "image");
    EXPECT_EQ(tasks[1].get_entry("aliases").get_value<std::string>("input"), "image");
}

TEST_F(WBERendGraphScriptVKResourceTest, RegisteredResourcesRejectWrongTypesWhenTasksAreCreated) {
    const DataValue recipe = RendGraphScriptVK::run("invalid_registered_keys",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local graphics = graph.graphics_pipeline("graphics")
    local compute = graph.compute_pipeline("compute")
    local mesh = graph.mesh("mesh")
    local invalid_calls = {
        function() graph.mesh("") end,
        function() graph.compute_pipeline("") end,
        function() graph.graphics_pipeline("") end,
        function() graph.compute({ pipeline = "compute" }) end,
        function() graph.compute({ pipeline = graphics }) end,
        function() graph.draw({ pipeline = compute }) end,
        function() graph.draw_mesh({ mesh = graphics, pipeline = { default = graphics }, parts = { "opaque" } }) end,
        function() graph.draw_mesh({ mesh = mesh, pipeline = { default = mesh }, parts = { "opaque" } }) end,
        function() graph.compute({ pipeline = graph.import("compute") }) end,
        function() graph.compute({ pipeline = { key = "compute", kind = "registered", dependencies = {},
            resource_type = compute.resource_type } }) end,
        function() graph.compute_sequential({ dispatches = { { pipeline = graphics } } }) end,
        function() graph.ping_pong({ ping_pipeline = compute, pong_pipeline = compute, iterations = 1,
            ping_prefix_pipeline = graphics, pong_prefix_pipeline = compute }) end,
        function() graph.task("registry_probe", { pipeline = compute }) end,
    }
    for _, call in ipairs(invalid_calls) do
        assert(not pcall(call))
    end
    return graph.build({ tasks = { graph.compute({ pipeline = compute }, { name = "valid" }) } })
end }
)",
        DataValue::make_map());
    ASSERT_EQ(recipe.get_entry("tasks").as_list().size(), 1U);
    EXPECT_EQ(recipe.get_entry("tasks").as_list()[0].get_value<std::string>("name"), "valid");
    EXPECT_TRUE(recipe.get_entry("imports").as_list().empty());
    EXPECT_TRUE(recipe.get_entry("resources").as_list().empty());
}

TEST_F(WBERendGraphScriptVKResourceTest, RegisteredResourcesRejectGraphBindingsBeforeChangingTasks) {
    const DataValue recipe = RendGraphScriptVK::run("registered_graph_bindings",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local compute = graph.compute_pipeline("compute")
    local graphics = graph.graphics_pipeline("graphics")
    local buffer = graph.storage_buffer({ name = "buffer", size = 16 })
    local producer = graph.compute({ pipeline = compute })
    local consumer = graph.draw({ pipeline = graphics })
    consumer:bind("valid", buffer)
    producer:connect(consumer, buffer)
    local bindings = consumer.bindings
    local edges = producer.edges
    for _, resource in ipairs({ graph.mesh("mesh"), compute, graphics }) do
        local invalid_calls = {
            function() graph.full_memory_barrier({ resource = resource }) end,
            function() graph.compute_sequential({ dispatches = { { pipeline = compute, indirect_buffer = resource } } }) end,
            function() graph.compute_sequential({ dispatches = { { pipeline = compute, bindings = { invalid = resource } } } }) end,
            function() graph.compute({ pipeline = compute }, { bindings = { invalid = resource } }) end,
            function() graph.subgraph({ tasks = {}, ports = { invalid = resource } }) end,
            function() graph.build({ tasks = {}, exports = { invalid = resource } }) end,
            function() consumer:bind("invalid", resource) end,
            function() producer:connect(consumer, resource) end,
            function() producer:connect(consumer, { buffer, resource }) end,
            function() producer:connect_all({ consumer }, { buffer, resource }) end,
            function() consumer:inputs_from(producer, { first = buffer, invalid = resource }) end,
            function() graph.chain({ buffer, resource }, { producer, consumer }) end,
            function() consumer:color_target(0, resource) end,
            function() consumer:depth_target(resource) end,
            function() resource:view() end,
            function() graph.image_view(resource) end,
        }
        for _, call in ipairs(invalid_calls) do
            assert(not pcall(call))
            assert(consumer.bindings == bindings and bindings.valid == buffer)
            local binding_count = 0
            for _ in pairs(bindings) do
                binding_count = binding_count + 1
            end
            assert(binding_count == 1)
            assert(producer.edges == edges and #edges == 1)
        end
    end
    return graph.build({ tasks = { producer, consumer } })
end }
)",
        DataValue::make_map());
    ASSERT_EQ(recipe.get_entry("tasks").as_list().size(), 2U);
    const auto& producer = recipe.get_entry("tasks").as_list()[0];
    ASSERT_EQ(producer.get_entry("connect").as_list().size(), 1U);
    EXPECT_EQ(producer.get_entry("connect").as_list()[0].get_value<std::string>("resource"), named_id(recipe, "buffer"));
    const auto& aliases = recipe.get_entry("tasks").as_list()[1].get_entry("aliases").as_map();
    EXPECT_EQ(aliases.size(), 1U);
    EXPECT_TRUE(recipe.get_entry("imports").as_list().empty());
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 1U);
}

TEST_F(WBERendGraphScriptVKResourceTest, BuildRejectsRegisteredResourcesWrittenIntoTaskHandles) {
    EXPECT_NO_THROW((void)RendGraphScriptVK::run("raw_registered_graph_bindings",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local invalid_bindings = {
        function(producer, consumer, buffer, resource)
            consumer.bindings = { input = resource }
        end,
        function(producer, consumer, buffer, resource)
            producer.edges = { { to = consumer, resource = resource } }
        end,
        function(producer, consumer, buffer, resource)
            buffer.dependencies = { resource }
            consumer:bind("input", buffer)
        end,
    }
    for _, resource_kind in ipairs({ "mesh", "compute", "graphics" }) do
        for _, bind in ipairs(invalid_bindings) do
            local resource = resource_kind == "mesh" and graph.mesh("resource") or
                resource_kind == "compute" and graph.compute_pipeline("resource") or graph.graphics_pipeline("resource")
            local buffer = graph.storage_buffer({ size = 16 })
            local producer = graph.task("registry_probe", nil)
            local consumer = graph.task("registry_probe", nil)
            bind(producer, consumer, buffer, resource)
            assert(not pcall(function() graph.build({ tasks = { producer, consumer } }) end))
        end
    end
    return {}
end }
)",
        DataValue::make_map()));
}

TEST_F(WBERendGraphScriptVKResourceTest, ImageMethodsCacheDefaultViewsAndPreserveExplicitRanges) {
    const DataValue recipe = RendGraphScriptVK::run("image_methods",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local image = graph.image({
        name = "image",
        usage = "image_read_write",
        format = "r16g16b16a16_sfloat",
        extent = { width = 16, height = 8 },
        mip_levels = 4,
    })
    local view = image:view()
    assert(image:view() == view)
    local range = image:view({ name = "image_range", base_mip_level = 1, mip_level_count = 3 })
    local draw = graph.draw({ pipeline = graph.graphics_pipeline("pipeline") }):color_target(0, view)
    local readback = graph.cpu_acquire_image({ image_view = range, event = "readback" })
    return graph.build({ tasks = { draw, readback } })
end }
)",
        DataValue::make_map());
    const auto& resources = recipe.get_entry("resources").as_list();
    ASSERT_EQ(resources.size(), 3U);
    // The default view takes the image's debug name.
    const std::string image = named_id(recipe, "image", "image");
    const std::string default_view = named_id(recipe, "image", "image_view");
    EXPECT_EQ(find_named_resource(recipe, "image", "image_view").get_entry("image_view").get_value<std::string>("image"), image);
    const DataValue& description = find_named_resource(recipe, "image_range").get_entry("image_view").get_entry("view");
    EXPECT_EQ(description.get_value<int64_t>("base_mip_level"), 1);
    EXPECT_EQ(description.get_value<int64_t>("mip_level_count"), 3);
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_context(&allocator);
    const auto graph = RendGraphRecipeBuilderVK().build(recipe, RendGraphRecipeImportsVK{}, &render_context, nullptr);
    ASSERT_EQ(graph.image_views.size(), 2U);
    // The no-argument method preserves the graph's existing default range of one mip and one layer.
    const auto default_description = std::ranges::find_if(graph.image_views, [&default_view](const auto& p_view) {
        return p_view.res_id == default_view;
    });
    ASSERT_NE(default_description, graph.image_views.end());
    const RendImageViewDescVK configuration = default_description->factory(RendGraphContextVK{});
    EXPECT_EQ(configuration.mip_level_count, 1U);
    EXPECT_EQ(configuration.layer_count, 1U);
}

TEST_F(WBERendGraphScriptVKResourceTest, TypedConstructorsDoNotMutateCallerParametersOrSharedOptions) {
    const DataValue recipe = RendGraphScriptVK::run("typed_parameter_copies",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local buffer = graph.storage_buffer({ name = "commands", size = 64 })
    local other = graph.storage_buffer({ name = "other", size = 64 })
    local options = { bindings = { input = buffer } }
    local params = { pipeline = graph.compute_pipeline("pipeline"), indirect_buffer = buffer }
    local first = graph.compute_indirect(params, options)
    assert(params.indirect_buffer == buffer)
    assert(options.bindings.input == buffer)
    params.indirect_buffer = other
    options.bindings.input = other
    local second = graph.compute_indirect(params, options)
    local dispatch = { pipeline = graph.compute_pipeline("nested"), indirect_buffer = buffer, bindings = { shader_buffer = buffer } }
    local sequence = graph.compute_sequential({ dispatches = { dispatch } })
    assert(dispatch.indirect_buffer == buffer)
    assert(dispatch.bindings.shader_buffer == buffer)
    dispatch.indirect_buffer = other
    dispatch.bindings.shader_buffer = other
    return graph.build({ tasks = { first, second, sequence } })
end }
)",
        DataValue::make_map());
    const std::string commands = named_id(recipe, "commands");
    const std::string other = named_id(recipe, "other");
    const auto& tasks = recipe.get_entry("tasks").as_list();
    EXPECT_EQ(tasks[0].get_entry("params").get_value<std::string>("indirect_buffer"), commands);
    EXPECT_EQ(tasks[0].get_entry("aliases").get_value<std::string>("input"), commands);
    EXPECT_EQ(tasks[1].get_entry("params").get_value<std::string>("indirect_buffer"), other);
    EXPECT_EQ(tasks[1].get_entry("aliases").get_value<std::string>("input"), other);
    const auto& dispatch = tasks[2].get_entry("params").get_entry("dispatches").as_list()[0];
    EXPECT_EQ(dispatch.get_value<std::string>("indirect_buffer"), commands);
    EXPECT_EQ(dispatch.get_entry("bindings").get_value<std::string>("shader_buffer"), commands);
}

TEST_F(WBERendGraphScriptVKResourceTest, AttachmentMethodsKeepAliasesTaskScopedAndRejectDuplicateTargets) {
    const DataValue recipe = RendGraphScriptVK::run("attachment_methods",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local first = graph.import("first")
    local second = graph.import("second")
    local depth = graph.import("depth")
    local draw = graph.draw({ pipeline = graph.graphics_pipeline("pipeline") })
    assert(draw:color_target(0, first) == draw)
    assert(draw:depth_target(depth) == draw)
    local other = graph.draw({ pipeline = graph.graphics_pipeline("pipeline") }):color_target(0, second)
    return graph.build({ tasks = { draw, other } })
end }
)",
        DataValue::make_map());
    const auto& tasks = recipe.get_entry("tasks").as_list();
    EXPECT_EQ(tasks[0].get_entry("aliases").get_value<std::string>("render_target_colors[0]"), "first");
    EXPECT_EQ(tasks[1].get_entry("aliases").get_value<std::string>("render_target_colors[0]"), "second");
    EXPECT_EQ(tasks[0].get_entry("aliases").get_value<std::string>("render_target_depth"), "depth");
    EXPECT_THROW((void)RendGraphScriptVK::run("duplicate_attachment",
                     R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local target = graph.import("target")
    graph.draw({ pipeline = graph.graphics_pipeline("pipeline") }):color_target(0, target):color_target(0, target)
    return graph.build({ tasks = {} })
end }
)",
                     DataValue::make_map()),
        std::runtime_error);
    EXPECT_THROW((void)RendGraphScriptVK::run("invalid_attachment_index",
                     R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    graph.draw({ pipeline = graph.graphics_pipeline("pipeline") }):color_target(-1, graph.import("target"))
    return graph.build({ tasks = {} })
end }
)",
                     DataValue::make_map()),
        std::runtime_error);
}

TEST_F(WBERendGraphScriptVKResourceTest, SequentialBindingsTakeHandlesAndRejectConflictingAliases) {
    const DataValue recipe = RendGraphScriptVK::run("sequential_identity_bindings",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local commands = graph.storage_buffer({ name = "commands", size = 64 })
    local input = graph.storage_buffer({ name = "input_buffer", size = 64 })
    return graph.build({ tasks = { graph.compute_sequential({ dispatches = {
        { pipeline = graph.compute_pipeline("first"), indirect_buffer = commands },
        { pipeline = graph.compute_pipeline("second"), bindings = { input = input } },
    } }) } })
end }
)",
        DataValue::make_map());
    const std::string commands = named_id(recipe, "commands");
    const std::string input = named_id(recipe, "input_buffer");
    const auto& task = recipe.get_entry("tasks").as_list()[0];
    EXPECT_EQ(task.get_entry("aliases").get_value<std::string>(commands), commands);
    EXPECT_EQ(task.get_entry("aliases").get_value<std::string>(input), input);
    const auto& dispatches = task.get_entry("params").get_entry("dispatches").as_list();
    EXPECT_EQ(dispatches[1].get_entry("bindings").get_value<std::string>("input"), input);
    // Bindings and indirect buffers take handles, not task-local names.
    for (const char* dispatch : {R"({ pipeline = graph.compute_pipeline("second"), bindings = { input = "input_buffer" } })",
             R"({ pipeline = graph.compute_pipeline("second"), indirect_buffer = "commands" })"}) {
        SCOPED_TRACE(dispatch);
        EXPECT_THROW((void)RendGraphScriptVK::run("string_binding",
                         std::string(R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    return graph.build({ tasks = { graph.compute_sequential({ dispatches = { )") +
                             dispatch + R"( } }) } })
end }
)",
                         DataValue::make_map()),
            std::runtime_error);
    }
}

TEST_F(WBERendGraphScriptVKResourceTest, RejectsViewsOfOwnedNonImageResources) {
    EXPECT_THROW((void)RendGraphScriptVK::run("buffer_view",
                     R"(
local graph = require("render_graph_utils.render_graph_utils")
return { build = function()
    local buffer = graph.storage_buffer({ size = 16 })
    return graph.build({ tasks = { graph.cpu_acquire_image({ image_view = buffer:view(), event = "readback" }) } })
end }
)",
                     DataValue::make_map()),
        std::runtime_error);
}

TEST_F(WBERendGraphScriptVKResourceTest, RunsTheShippedPixelSortingRecipe) {
    RenderAllocator allocator(WBE_KI_B(16));
    RenderContextVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    DataValue context = RendGraphScriptVK::make_context(render_data, RendGraphContextVK{});
    context.set_value("source_width", 64);
    context.set_value("source_height", 32);
    context.set_value("orientation", std::string("horizontal"));
    const DataValue recipe = RendGraphScriptVK::run("render_graphs.pixel_sorting", context);
    // 3 line passes, the radix sort, 2 image passes, present, and the imported ImGui task.
    const DataValue::List& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 8U);
    EXPECT_EQ(tasks.back().get_value<std::string>("id"), "draw_imgui");
    EXPECT_TRUE(tasks.back().get_value<bool>("import"));
    // The radix sort is one task dispatching the preparation and 5 passes for each of its 4 digits.
    const DataValue& radix_sort = find_named_task(recipe, "radix_sort");
    EXPECT_EQ(radix_sort.get_value<std::string>("type"), "run_compute_pipelines_sequential");
    EXPECT_EQ(radix_sort.get_entry("params").get_entry("dispatches").as_list().size(), 21U);
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 12U);
    // The application reads the result back through the exported image.
    EXPECT_EQ(recipe.get_entry("exports").get_value<std::string>("post_processing_output_image"),
        named_id(recipe, "pixel_sorting_post_processing_output", "image"));
}

TEST_F(WBERendGraphScriptVKResourceTest, RunsTheShippedMaterialVariantsRecipeAndLowersVariantPipelines) {
    RenderAllocator allocator(WBE_KI_B(16));
    RenderContextVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    const DataValue recipe = RendGraphScriptVK::run(
        "render_graphs.draw_material_variants", RendGraphScriptVK::make_context(render_data, RendGraphContextVK{}));
    const DataValue::List& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 1U);
    const DataValue& params = tasks[0].get_entry("params");
    const DataValue& pipelines = params.get_entry("pipeline");
    ASSERT_TRUE(pipelines.get_kind() == DataValueKind::MAP);
    EXPECT_EQ(pipelines.get_value<std::string>("default"), "draw_cube_unshaded_pipeline");
    EXPECT_EQ(pipelines.get_value<std::string>("tinted"), "draw_cube_unshaded_tinted_pipeline");
    // The recipe registers both variant pipelines for the swap chain format.
    const DataValue::List& registrations = recipe.get_entry("registrations").as_list();
    ASSERT_EQ(registrations.size(), 2U);
    for (const DataValue& registration : registrations) {
        EXPECT_EQ(registration.get_value<std::string>("type"), "RDParamsGraphicsPipelineVK");
        EXPECT_EQ(registration.get_entry("description").get_entry("color_formats").as_list().size(), 1U);
    }
}

TEST_F(WBERendGraphScriptVKResourceTest, RunsTheShippedForwardRecipes) {
    RenderAllocator allocator(WBE_KI_B(16));
    RenderContextVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    const DataValue base_context =
        RendGraphScriptVK::make_context(render_data, RendGraphContextVK{.swap_chain_extent = {.width = 64, .height = 32}});
    DataValue shadowed_context = base_context;
    DataValue shadow_extent = DataValue::make_map();
    shadow_extent.set_value("width", 128);
    shadow_extent.set_value("height", 128);
    shadowed_context.set_entry("shadow_extent", shadow_extent);
    shadowed_context.set_value("shadow_color_format", std::string("r8_unorm"));
    shadowed_context.set_value("shadow_depth_format", std::string("d32_sfloat"));
    shadowed_context.set_value("depth_format", std::string("d32_sfloat"));
    // The debug names of the tasks, or the ids of imported ones.
    const auto task_names = [](const DataValue& p_recipe) {
        std::vector<std::string> result;
        for (const DataValue& task : p_recipe.get_entry("tasks").as_list()) {
            result.push_back(task.get_value<std::string>(task.contains("name") ? "name" : "id"));
        }
        return result;
    };
    const auto imports = [](const DataValue& p_recipe) {
        std::vector<std::string> result;
        for (const DataValue& import : p_recipe.get_entry("imports").as_list()) {
            result.push_back(import.as_string());
        }
        std::ranges::sort(result);
        return result;
    };
    // Every recipe registers the meshes and pipelines its draws name, and imports only the swap chain and the application's
    // buffers and tasks.
    const DataValue triangle = RendGraphScriptVK::run("render_graphs.draw_triangle", base_context);
    EXPECT_EQ(task_names(triangle), (std::vector<std::string>{"draw_triangle"}));
    EXPECT_EQ(imports(triangle), (std::vector<std::string>{"swap_chain"}));
    EXPECT_EQ(triangle.get_entry("registrations").as_list().size(), 2U);
    const DataValue cube = RendGraphScriptVK::run("render_graphs.draw_cube", shadowed_context);
    EXPECT_EQ(task_names(cube), (std::vector<std::string>{"draw_shadow_map", "draw_main"}));
    EXPECT_EQ(imports(cube),
        (std::vector<std::string>{"camera_info", "shadow_light_info", "spot_light_count", "spot_light_info", "swap_chain"}));
    EXPECT_EQ(cube.get_entry("registrations").as_list().size(), 3U);
    const DataValue sponza = RendGraphScriptVK::run("render_graphs.draw_sponza", shadowed_context);
    EXPECT_EQ(task_names(sponza),
        (std::vector<std::string>{
            "draw_shadow_map_opaque", "draw_shadow_map_masked", "draw_main_opaque", "draw_main_masked", "draw_light_billboard"}));
    EXPECT_EQ(imports(sponza),
        (std::vector<std::string>{
            "camera_info", "gbuffer_camera_info", "light_billboard_info", "light_billboard_texture", "shadow_light_info", "spot_light_count", "spot_light_info", "swap_chain"}));
    // The mesh, five pipelines, and the billboard texture.
    EXPECT_EQ(sponza.get_entry("registrations").as_list().size(), 7U);
    const DataValue skinned = RendGraphScriptVK::run("render_graphs.draw_skinned_mesh", base_context);
    EXPECT_EQ(task_names(skinned), (std::vector<std::string>{"draw_skinned_mesh", "draw_skinned_mesh_imgui"}));
    EXPECT_TRUE(skinned.get_entry("tasks").as_list()[1].get_value<bool>("import"));
    EXPECT_EQ(imports(skinned), (std::vector<std::string>{"skinned_mesh_camera_info", "skinned_mesh_skinning_transforms", "swap_chain"}));
}

TEST_F(WBERendGraphScriptVKResourceTest, BuildsTheSVO3LeafDeduplicationAsOneSequentialTask) {
    const DataValue recipe = RendGraphScriptVK::run("tests.teal_svo_3", DataValue::make_map());
    // The deferred-list reset, the gathering, and one task clamping the leaf count and dispatching the deduplication.
    const DataValue::List& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 3U);
    const std::string deferred_list = named_id(recipe, "svo_3_leaves.keys");
    const DataValue& gather = find_named_task(recipe, "svo_3_gather_leaves");
    EXPECT_EQ(gather.get_value<std::string>("type"), "graphics_draw_mesh");
    EXPECT_EQ(gather.get_entry("aliases").get_value<std::string>("svo_deferred_list"), deferred_list);
    const std::string deduplicate_id = named_task_id(recipe, "svo_3_deduplicate_leaves");
    EXPECT_TRUE(std::ranges::any_of(gather.get_entry("connect").as_list(), [&](const DataValue& p_connection) {
        return p_connection.get_value<std::string>("to") == deduplicate_id && p_connection.get_value<std::string>("resource") == deferred_list;
    }));
    const DataValue& deduplicate = find_named_task(recipe, "svo_3_deduplicate_leaves");
    EXPECT_EQ(deduplicate.get_value<std::string>("type"), "run_compute_pipelines_sequential");
    // The clamping, the deduplication's preparation, the radix sort's preparation and 5 passes for each of its 4 digits,
    // the marking, the scan's preparation and 4 passes, and the scatter.
    const DataValue::List& dispatches = deduplicate.get_entry("params").get_entry("dispatches").as_list();
    ASSERT_EQ(dispatches.size(), 30U);
    const auto bound = [&](const DataValue& p_dispatch, std::string_view p_name) {
        return p_dispatch.get_entry("bindings").get_value<std::string>(p_name);
    };
    EXPECT_EQ(dispatches[0].get_value<std::string>("pipeline"), "svo_3_prepare_leaves_shader");
    EXPECT_EQ(dispatches[1].get_value<std::string>("pipeline"), "dedup_large_prepare_dispatch");
    EXPECT_EQ(dispatches[2].get_value<std::string>("pipeline"), "radix_sort_prepare_dispatch");
    EXPECT_EQ(bound(dispatches[2], "input_buffer"), deferred_list);
    const DataValue& mark = dispatches[23];
    EXPECT_EQ(mark.get_value<std::string>("pipeline"), "dedup_large_mark");
    EXPECT_EQ(mark.get_value<std::string>("indirect_buffer"), named_id(recipe, "svo_3_leaves.dedup_dispatch"));
    EXPECT_EQ(bound(mark, "marks"), named_id(recipe, "svo_3_leaves.marks"));
    // The scan's blocks pass scans the marks in place, and its sections pass runs the fixed 256 workgroups.
    EXPECT_EQ(dispatches[25].get_value<std::string>("pipeline"), "large_prefix_scan_blocks");
    EXPECT_EQ(bound(dispatches[25], "data_buffer"), named_id(recipe, "svo_3_leaves.marks"));
    EXPECT_EQ(dispatches[25].get_value<std::string>("indirect_buffer"), named_id(recipe, "svo_3_leaves.mark_scan.dispatch"));
    EXPECT_EQ(dispatches[26].get_value<uint32_t>("group_count_x"), 256U);
    EXPECT_EQ(dispatches[29].get_value<std::string>("pipeline"), "dedup_large_scatter");
    EXPECT_EQ(bound(dispatches[29], "unique_keys"), named_id(recipe, "svo_3_leaves.unique_keys"));
    // Every resource a dispatch names is bound to itself, since recipe tasks bind every resource they use.
    const DataValue& aliases = deduplicate.get_entry("aliases");
    EXPECT_EQ(aliases.get_value<std::string>(deferred_list), deferred_list);
}

TEST_F(WBERendGraphScriptVKResourceTest, RadixSortsUInt2KeysByEveryBitOrABitRange) {
    const DataValue result = RendGraphScriptVK::run("radix_sort_uint2",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
local sort = require("render_graph_utils.sort")
return { build = function()
    local buffers = sort.radix_sort_buffers({ name = "sort", max_element_count = 1000, key_type = "uint2" })
    return graph.build({ tasks = {
        sort.radix_sort_task(buffers, { name = "full" }),
        sort.radix_sort_task(buffers, { name = "key_value", begin_bit = 32 }),
        sort.radix_sort_key_value_task(buffers, { name = "key_value_task", queue_family = "compute_1" }),
    } })
end }
)",
        DataValue::make_map());
    const DataValue::List& tasks = result.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 3U);
    const auto pipeline = [&](size_t p_task, size_t p_dispatch) {
        return tasks[p_task].get_entry("params").get_entry("dispatches").as_list()[p_dispatch].get_value<std::string>("pipeline");
    };
    // The preparation and 5 passes for each of the 8 digits, or of the 4 digits of x.
    EXPECT_EQ(tasks[0].get_entry("params").get_entry("dispatches").as_list().size(), 41U);
    EXPECT_EQ(pipeline(0, 0), "radix_sort_prepare_dispatch");
    EXPECT_EQ(pipeline(0, 1), "radix_sort_uint2_histogram_workgroup_process_0_8");
    EXPECT_EQ(pipeline(0, 2), "radix_sort_histogram_prefix_sum");
    EXPECT_EQ(pipeline(0, 5), "radix_sort_uint2_scatter_0_8");
    EXPECT_EQ(pipeline(0, 40), "radix_sort_uint2_scatter_56_64");
    EXPECT_EQ(tasks[1].get_entry("params").get_entry("dispatches").as_list().size(), 21U);
    EXPECT_EQ(pipeline(1, 1), "radix_sort_uint2_histogram_workgroup_process_32_40");
    EXPECT_EQ(pipeline(1, 20), "radix_sort_uint2_scatter_56_64");
    // The key-value task runs the same dispatches as the bits [32, 64) sort, on its queue family.
    EXPECT_EQ(tasks[2].get_entry("params").get_entry("dispatches"), tasks[1].get_entry("params").get_entry("dispatches"));
    EXPECT_EQ(tasks[2].get_entry("params").get_value<std::string>("queue_family"), "compute_1");
    // The metadata and 1000 keys of 8 bytes each.
    EXPECT_EQ(find_named_resource(result, "sort.keys").get_entry("buffer").get_value<int64_t>("size"), 8016);
    EXPECT_EQ(find_named_resource(result, "sort.keys_swap").get_entry("buffer").get_value<int64_t>("size"), 8016);
}

TEST_F(WBERendGraphScriptVKResourceTest, RadixSortsUIntKeysByABitRange) {
    const DataValue result = RendGraphScriptVK::run("radix_sort_uint_range",
        R"(
local graph = require("render_graph_utils.render_graph_utils")
local sort = require("render_graph_utils.sort")
return { build = function()
    local buffers = sort.radix_sort_buffers({ name = "sort", max_element_count = 1000 })
    return graph.build({ tasks = { sort.radix_sort_task(buffers, { begin_bit = 8, end_bit = 24 }) } })
end }
)",
        DataValue::make_map());
    const DataValue& task = result.get_entry("tasks").as_list().at(0);
    const DataValue::List& dispatches = task.get_entry("params").get_entry("dispatches").as_list();
    ASSERT_EQ(dispatches.size(), 11U);
    EXPECT_EQ(dispatches[1].get_value<std::string>("pipeline"), "radix_sort_histogram_workgroup_process_8_16");
    EXPECT_EQ(dispatches[10].get_value<std::string>("pipeline"), "radix_sort_scatter_16_24");
    // The second pass scatters back into the key buffer.
    EXPECT_EQ(dispatches[10].get_entry("bindings").get_value<std::string>("output_buffer"), named_id(result, "sort.keys"));
}

TEST_F(WBERendGraphScriptVKResourceTest, RadixSortsRejectInvalidBitRanges) {
    const auto run_sort = [](std::string_view p_key_type, std::string_view p_options) {
        const std::string source = std::format(R"(
local graph = require("render_graph_utils.render_graph_utils")
local sort = require("render_graph_utils.sort")
return {{ build = function()
    local buffers = sort.radix_sort_buffers({{ max_element_count = 1000, key_type = "{}" }})
    return graph.build({{ tasks = {{ sort.radix_sort_task(buffers, {}) }} }})
end }}
)",
            p_key_type,
            p_options);
        return RendGraphScriptVK::run("radix_sort_range", source, DataValue::make_map());
    };
    EXPECT_NO_THROW((void)run_sort("uint2", "{ begin_bit = 32 }"));
    EXPECT_NO_THROW((void)run_sort("uint", "{ begin_bit = 16 }"));
    for (const std::string_view options :
        {"{ begin_bit = 4 }", "{ end_bit = 60 }", "{ begin_bit = -16 }", "{ end_bit = 72 }", "{ begin_bit = 32, end_bit = 32 }", "{ begin_bit = 40 }"}) {
        EXPECT_THROW((void)run_sort("uint2", options), std::runtime_error) << options;
    }
    // A range past the 32 bits of uint keys.
    EXPECT_THROW((void)run_sort("uint", "{ begin_bit = 32, end_bit = 48 }"), std::runtime_error);
    // The key-value sort orders the x of uint2 keys, so it takes neither uint keys nor a bit range.
    for (const char* call : {"sort.radix_sort_key_value_task(sort.radix_sort_buffers({ max_element_count = 1000 }))",
             R"(sort.radix_sort_key_value_task(sort.radix_sort_buffers({ max_element_count = 1000, key_type = "uint2" }), { begin_bit = 40 }))"}) {
        SCOPED_TRACE(call);
        EXPECT_THROW((void)RendGraphScriptVK::run("radix_sort_key_value",
                         std::format(R"(
local graph = require("render_graph_utils.render_graph_utils")
local sort = require("render_graph_utils.sort")
return {{ build = function()
    return graph.build({{ tasks = {{ {} }} }})
end }}
)",
                             call),
                         DataValue::make_map()),
            std::runtime_error);
    }
}

TEST_F(WBERendGraphScriptVKResourceTest, RunsTheShippedDeferredRecipeWithItsVoxelAndPixelSortFeatures) {
    RenderAllocator allocator(WBE_KI_B(16));
    RenderContextVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    auto make_extent = [](uint32_t p_width, uint32_t p_height) {
        DataValue extent = DataValue::make_map();
        extent.set_value("width", p_width);
        extent.set_value("height", p_height);
        return extent;
    };
    auto make_octree = [&] {
        DataValue octree = DataValue::make_map();
        octree.set_value("module", std::string("render_graph_utils.features.voxel_octree"));
        octree.set_value("level_count", 2);
        DataValue leaf_groups = DataValue::make_map();
        leaf_groups.set_value("x", 4);
        leaf_groups.set_value("y", 4);
        leaf_groups.set_value("z", 2);
        octree.set_entry("leaf_gather_group_count", leaf_groups);
        octree.set_value("node_gather_group_count", 8);
        octree.set_value("leaf_draw_count", 33);
        octree.set_value("extent", 64);
        DataValue static_build = DataValue::make_map();
        static_build.set_value("event", std::string("test.voxel_static_build"));
        static_build.set_value("activate", true);
        octree.set_entry("static_build", static_build);
        return octree;
    };
    auto make_svo = [&] {
        DataValue svo = DataValue::make_map();
        svo.set_value("module", std::string("render_graph_utils.features.voxel_svo"));
        svo.set_value("variant", std::string("pass_published"));
        svo.set_value("shader_prefix", std::string("svo_2"));
        svo.set_value("dynamic_shader_prefix", std::string("svo_2_dynamic"));
        svo.set_value("extent", 512);
        svo.set_value("max_depth", 9);
        DataValue buffer_sizes = DataValue::make_map();
        buffer_sizes.set_value("deferred_list", 516);
        buffer_sizes.set_value("pre_deferred_debug_info", 60);
        buffer_sizes.set_value("pre_deferred_debug_list", 512);
        buffer_sizes.set_value("occupancy", 1024);
        buffer_sizes.set_value("radiance_index", 528);
        buffer_sizes.set_value("radiance_staged", 4096);
        buffer_sizes.set_value("debug_draw_node_list", 256);
        svo.set_entry("buffer_sizes", buffer_sizes);
        for (const char* group_count : {
                 "reset_group_count",
                 "deferred_dispatch_group_count",
                 "dynamic_clear_group_count",
                 "static_publication_group_count",
                 "dynamic_publication_group_count",
                 "occupancy_injection_group_count_x",
                 "occupancy_injection_group_count_y",
                 "occupancy_propagation_group_count",
                 "radiance_injection_group_count",
                 "debug_draw_node_search_group_count",
             }) {
            svo.set_value(group_count, 4);
        }
        DataValue static_build = DataValue::make_map();
        static_build.set_value("event", std::string("test.voxel_static_build"));
        static_build.set_value("activate", true);
        svo.set_entry("static_build", static_build);
        return svo;
    };
    auto run = [&](bool p_summed_area_table, bool p_post_processing, DataValue p_voxel) {
        DataValue context =
            RendGraphScriptVK::make_context(render_data, RendGraphContextVK{.swap_chain_extent = {.width = 640, .height = 360}});
        context.set_value("spot_light_count", 2);
        context.set_value("max_spot_light_info_count", 4);
        context.set_entry("shadow_extent", make_extent(256, 256));
        context.set_value("shadow_mipmap_level_count", 9);
        context.set_entry("bloom_extent", make_extent(640, 360));
        DataValue formats = DataValue::make_map();
        for (const char* role : {
                 "gbuffer_albedo",
                 "gbuffer_normal",
                 "gbuffer_rma",
                 "gbuffer_depth",
                 "shadowmap_depth",
                 "shadow_moments",
                 "rsm_flux",
                 "rsm_normal",
                 "lighting_output",
             }) {
            formats.set_value(role, std::string("r16g16b16a16_sfloat"));
        }
        context.set_entry("formats", formats);
        context.set_value("bloom_acquire_event", std::string("test.bloom"));
        context.set_value("bloom_image_event", std::string("test.bloom_image"));
        context.set_value("frame_sequence_event", std::string("test.frames"));
        context.set_value("has_mesh_acceleration_structure", false);
        DataValue static_mesh = DataValue::make_map();
        static_mesh.set_value("mesh", std::string("sponza.mesh"));
        context.set_entry("static_mesh", static_mesh);
        DataValue dynamic_mesh = DataValue::make_map();
        dynamic_mesh.set_value("mesh", std::string("model_cube.mesh"));
        context.set_entry("dynamic_mesh", dynamic_mesh);
        context.set_entry("voxel", p_voxel);
        DataValue entities = DataValue::make_list();
        entities.push_back(DataValue::make_string("dynamic_mesh_entity_0"));
        entities.push_back(DataValue::make_string("dynamic_mesh_entity_1"));
        context.set_entry("dynamic_mesh_entity_ids", entities);
        context.set_value("dynamic_mesh_transform_member", std::string("transform"));
        DataValue toggles = DataValue::make_map();
        for (const char* group : {"enabled", "tree_view", "pre_deferred_propagation"}) {
            DataValue toggle = DataValue::make_map();
            toggle.set_value("event", std::string("test.voxel_debug_view.") + group);
            toggle.set_value("enabled", false);
            toggles.set_entry(group, toggle);
        }
        DataValue lighting = DataValue::make_map();
        lighting.set_value("event", std::string("test.voxel_debug_view.lighting"));
        lighting.set_value("branch", 0);
        toggles.set_entry("lighting", lighting);
        DataValue tree_draw = DataValue::make_map();
        tree_draw.set_value("event", std::string("test.voxel_debug_view.tree_draw"));
        tree_draw.set_value("branch", 0);
        toggles.set_entry("tree_draw", tree_draw);
        context.set_entry("voxel_debug_view_toggles", toggles);
        if (p_post_processing) {
            DataValue post_processing = DataValue::make_map();
            post_processing.set_value("module", std::string("render_graph_utils.features.pixel_sort"));
            post_processing.set_value("direction", std::string("vertical"));
            post_processing.set_value("scope", std::string("span"));
            post_processing.set_value("color_format", std::string("r16g16b16a16_sfloat"));
            post_processing.set_value("queue_family", std::string("compute_1"));
            post_processing.set_value("contrast_import", std::string("pixel_sort_contrast"));
            post_processing.set_value("toggle_event", std::string("test.pixel_sort"));
            post_processing.set_value("enabled", true);
            context.set_entry("post_processing", post_processing);
        }
        // The render graph definitions set the recipe's static parameters.
        DataValue parameters = DataValue::make_map();
        parameters.set_value("summed_area_table", p_summed_area_table);
        parameters.set_value("bloom_blur_pass_count", 4);
        return RendGraphScriptVK::run("render_graphs.deferred", context, parameters);
    };
    const auto count_tasks_with = [](const DataValue::List& p_tasks, std::string_view p_field) {
        size_t count = 0;
        for (const DataValue& task : p_tasks) {
            count += task.contains(p_field) ? 1U : 0U;
        }
        return count;
    };
    const auto contains_import = [](const DataValue& p_recipe, std::string_view p_name) {
        const DataValue::List& imports = p_recipe.get_entry("imports").as_list();
        return std::ranges::any_of(imports, [p_name](const DataValue& p_import) {
            return p_import.as_string() == p_name;
        });
    };
    const auto find_by_name = [](const DataValue::List& p_entries, std::string_view p_name) -> const DataValue* {
        for (const DataValue& entry : p_entries) {
            if (entry.contains("name") && entry.get_value<std::string>("name") == p_name) {
                return &entry;
            }
        }
        return nullptr;
    };
    // 2 lights x (4 shadow mesh draws + 1 filter), 4 G-buffer mesh draws, indirect lighting multiplexing its debug view,
    // the frame sequence acquisition, ImGui, direct and combine lighting, extract and blur bloom, the bloom acquisition, 2
    // presents, 2 billboards, and the voxel subgraph.
    constexpr size_t BASE_TASK_COUNT = 10 + 4 + 1 + 1 + 1 + 2 + 2 + 1 + 2 + 2 + 1;
    const DataValue mipmap = run(false, false, make_octree());
    const DataValue::List& mipmap_tasks = mipmap.get_entry("tasks").as_list();
    EXPECT_EQ(mipmap_tasks.size(), BASE_TASK_COUNT);
    EXPECT_EQ(count_tasks_with(mipmap_tasks, "toggle"), 0U);
    // Indirect lighting is one multiplexer of the lit result and the debug view, which share its aliases.
    const DataValue* indirect_lighting = find_by_name(mipmap_tasks, "run_indirect_lighting");
    ASSERT_NE(indirect_lighting, nullptr);
    const DataValue::List& indirect_branches = indirect_lighting->get_entry("branches").as_list();
    ASSERT_EQ(indirect_branches.size(), 2U);
    EXPECT_EQ(indirect_branches[1].get_entry("params").get_value<std::string>("pipeline"), "indirect_lighting_rsm_debug_view_shader");
    EXPECT_EQ(indirect_lighting->get_entry("select").get_value<std::string>("event"), "test.voxel_debug_view.lighting");
    EXPECT_EQ(indirect_lighting->get_entry("aliases").get_value<std::string>("shadowmap_color[1]"),
        named_id(mipmap, "shadowmap_color_1", "image_view"));
    // The dynamic draws bind the transform members of the entity imports.
    const DataValue* dynamic_gbuffer = find_by_name(mipmap_tasks, "dynamic_gbuffer");
    ASSERT_NE(dynamic_gbuffer, nullptr);
    EXPECT_EQ(dynamic_gbuffer->get_entry("params").get_value<std::string>("transform"), "dynamic_mesh_entity_0.transform");
    EXPECT_TRUE(contains_import(mipmap, "dynamic_mesh_entity_0"));
    // The frame sequence acquisition, imported from C++, is bound to the recipe's frame sequence view.
    EXPECT_EQ(find_by_name(mipmap_tasks, "acquire_frame_sequence"), nullptr);
    const auto acquire_frame_sequence = std::ranges::find_if(mipmap_tasks, [](const DataValue& p_task) {
        return p_task.get_value<std::string>("id") == "acquire_frame_sequence";
    });
    ASSERT_NE(acquire_frame_sequence, mipmap_tasks.end());
    EXPECT_EQ(acquire_frame_sequence->get_entry("aliases").get_value<std::string>("frame_sequence"),
        named_id(mipmap, "frame_sequence", "image_view"));
    EXPECT_EQ(find_by_name(mipmap_tasks, "acquire_bloom")->get_entry("params").get_value<std::string>("event"), "test.bloom_image");
    EXPECT_EQ(count_tasks_with(mipmap_tasks, "trigger"), 2U);
    // The octree subgraph holds its frame-begin barrier, 2 imported C++ clears, 4 construction draws, and 2 gathers and 2
    // overlays, all toggled, and imports the application's textures and buffers.
    const DataValue* octree = find_by_name(mipmap_tasks, "voxel");
    ASSERT_NE(octree, nullptr);
    const DataValue& octree_recipe = octree->get_entry("subgraph");
    EXPECT_EQ(octree_recipe.get_entry("tasks").as_list().size(), 11U);
    EXPECT_EQ(count_tasks_with(octree_recipe.get_entry("tasks").as_list(), "toggle"), 4U);
    // The static construction draws are triggered; the imported static clear carries its trigger in C++.
    EXPECT_EQ(count_tasks_with(octree_recipe.get_entry("tasks").as_list(), "trigger"), 2U);
    const DataValue* octree_build = find_by_name(octree_recipe.get_entry("tasks").as_list(), "run_build_octree");
    ASSERT_NE(octree_build, nullptr);
    EXPECT_EQ(octree_build->get_entry("aliases").get_value<std::string>("octree_static_levels[1]"), "octree_static_levels[1]");
    ASSERT_NE(octree_build, nullptr);
    EXPECT_EQ(octree_build->get_value<std::string>("type"), "graphics_draw_mesh");
    EXPECT_TRUE(contains_import(octree_recipe, "octree_dynamic"));
    EXPECT_EQ(octree->get_entry("ports").get_value<std::string>("depth"), named_id(mipmap, "gbuffer_depth", "image_view"));
    // The pass-published SVO subgraph holds 20 tasks, 4 of them the SVOGI occupancy injection and propagation and the
    // radiance index reset and injection: the 5 static construction tasks are triggered, and the pre-deferred capture, the
    // debug view's node search, and the 2 overlays toggled. The pixel sort subgraph holds 6 tasks, and its one toggle gates them all.
    const DataValue summed_area_table = run(true, true, make_svo());
    const DataValue::List& tasks = summed_area_table.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), BASE_TASK_COUNT + 1);
    EXPECT_EQ(count_tasks_with(tasks, "toggle"), 1U);
    const DataValue* svo = find_by_name(tasks, "voxel");
    ASSERT_NE(svo, nullptr);
    const DataValue& svo_recipe = svo->get_entry("subgraph");
    const DataValue::List& svo_tasks = svo_recipe.get_entry("tasks").as_list();
    EXPECT_EQ(svo_tasks.size(), 20U);
    EXPECT_EQ(count_tasks_with(svo_tasks, "toggle"), 4U);
    EXPECT_EQ(count_tasks_with(svo_tasks, "trigger"), 5U);
    EXPECT_TRUE(contains_import(svo_recipe, "svo_node_buffer"));
    // The SVO declares its 4 construction, 3 SVOGI, and 1 debug node list scratch buffers with the sizes it is given.
    const DataValue::List& svo_resources = svo_recipe.get_entry("resources").as_list();
    EXPECT_EQ(svo_resources.size(), 8U);
    const DataValue* node_list = find_by_name(svo_resources, "svo_debug_draw_node_list");
    const std::string occupancy = named_id(svo_recipe, "svogi_occupancy_buffer");
    const std::string tree_draw_id = named_task_id(svo_recipe, "run_svo_debug_draw");
    ASSERT_NE(node_list, nullptr);
    EXPECT_EQ(node_list->get_entry("buffer").get_value<int64_t>("size"), 256);
    // The tree draw multiplexes every style and coloring, and follows the occupancy propagation it may read.
    const DataValue* tree_draw = find_by_name(svo_tasks, "run_svo_debug_draw");
    ASSERT_NE(tree_draw, nullptr);
    EXPECT_EQ(tree_draw->get_entry("branches").as_list().size(), 4U);
    EXPECT_EQ(tree_draw->get_entry("select").get_value<std::string>("event"), "test.voxel_debug_view.tree_draw");
    const DataValue* propagation = find_by_name(svo_tasks, "propagate_svogi_occupancy");
    ASSERT_NE(propagation, nullptr);
    EXPECT_TRUE(std::ranges::any_of(propagation->get_entry("connect").as_list(), [&](const DataValue& p_connection) {
        return p_connection.get_value<std::string>("to") == tree_draw_id && p_connection.get_value<std::string>("resource") == occupancy;
    }));
    const DataValue* deferred_list = find_by_name(svo_resources, "svo_deferred_list_ping");
    ASSERT_NE(deferred_list, nullptr);
    EXPECT_EQ(deferred_list->get_entry("buffer").get_value<int64_t>("size"), 516);
    const DataValue* pre_deferred_debug_list = find_by_name(svo_resources, "svo_pre_deferred_debug_list_buffer");
    ASSERT_NE(pre_deferred_debug_list, nullptr);
    EXPECT_EQ(pre_deferred_debug_list->get_entry("buffer").get_value<int64_t>("size"), 512);
    const DataValue* radiance_staged = find_by_name(svo_resources, "svogi_radiance_staged_buffer");
    ASSERT_NE(radiance_staged, nullptr);
    EXPECT_EQ(radiance_staged->get_entry("buffer").get_value<int64_t>("size"), 4096);
    // The radiance injection binds the reflective-shadow-map slots of the enclosing graph through ports of their names;
    // slots past the light count reuse light zero.
    const DataValue* radiance_injection = find_by_name(svo_tasks, "inject_svogi_radiance");
    ASSERT_NE(radiance_injection, nullptr);
    const DataValue& radiance_aliases = radiance_injection->get_entry("aliases");
    EXPECT_EQ(radiance_aliases.get_value<std::string>("shadowmap_color[1]"), "shadowmap_color[1]");
    EXPECT_EQ(radiance_aliases.get_value<std::string>("index_buffer"), named_id(svo_recipe, "svogi_radiance_index_buffer"));
    EXPECT_EQ(svo->get_entry("ports").get_value<std::string>("shadowmap_color[1]"),
        named_id(summed_area_table, "shadowmap_color_1", "image_view"));
    // Only mip level zero of the moments is loaded, so the mip-chain slot binds the base moments view.
    EXPECT_EQ(svo->get_entry("ports").get_value<std::string>("shadow_moments_sampled[3]"),
        named_id(summed_area_table, "shadow_moments_0", "image_view"));
    EXPECT_EQ(svo->get_entry("ports").get_value<std::string>("shadow_sampler"), named_id(summed_area_table, "shadow_sampler"));
    // The subgraph follows every light's last shadow pass on the reflective shadow maps it reads.
    const DataValue* dynamic_shadow = find_by_name(tasks, "dynamic_shadow_second_1");
    ASSERT_NE(dynamic_shadow, nullptr);
    const std::string shadowmap_color = named_id(summed_area_table, "shadowmap_color_1", "image_view");
    EXPECT_TRUE(std::ranges::any_of(dynamic_shadow->get_entry("connect").as_list(), [&](const DataValue& p_connection) {
        return p_connection.get_value<std::string>("to") == svo->get_value<std::string>("id") &&
               p_connection.get_value<std::string>("resource") == shadowmap_color;
    }));
    const DataValue* pixel_sort = find_by_name(tasks, "post_processing");
    ASSERT_NE(pixel_sort, nullptr);
    EXPECT_TRUE(pixel_sort->contains("toggle"));
    const DataValue& pixel_sort_recipe = pixel_sort->get_entry("subgraph");
    EXPECT_EQ(pixel_sort_recipe.get_entry("tasks").as_list().size(), 6U);
    EXPECT_EQ(count_tasks_with(pixel_sort_recipe.get_entry("tasks").as_list(), "toggle"), 0U);
    EXPECT_TRUE(contains_import(pixel_sort_recipe, "pixel_sort_contrast"));
    EXPECT_EQ(pixel_sort->get_entry("ports").get_value<std::string>("color"), named_id(summed_area_table, "result_image", "image_view"));
}

} // namespace WhiteBirdEngine
