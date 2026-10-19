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
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "global/global.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "test_global.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_UNORM;
    const DataValue recipe = RendGraphScriptVK::run(
        "test_recipe", TEST_RECIPE_SCRIPT, RendGraphScriptVK::make_context(render_data, RendGraphContextVK{}));
    RendGraphDescVK graph = RendGraphRecipeBuilderVK().build(recipe, RendGraphRecipeImportsVK{}, &render_data);
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
    BaseRenderDataVK render_data(&allocator);
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

TEST_F(WBERendGraphScriptVKResourceTest, RunsTheShippedPixelSortingRecipe) {
    RenderAllocator allocator(WBE_KI_B(16));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    DataValue context = RendGraphScriptVK::make_context(render_data, RendGraphContextVK{});
    context.set_value("source_width", 64);
    context.set_value("source_height", 32);
    context.set_value("orientation", std::string("horizontal"));
    const DataValue recipe = RendGraphScriptVK::run("render_graphs.pixel_sorting", context);
    // 3 line passes, the dispatch preparation, 5 tasks for each of the 4 radix sort passes, 2 image passes, present, and
    // the imported ImGui task.
    const DataValue::List& tasks = recipe.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), 28U);
    EXPECT_EQ(tasks.back().get_value<std::string>("id"), "draw_imgui");
    EXPECT_TRUE(tasks.back().get_value<bool>("import"));
    EXPECT_EQ(recipe.get_entry("resources").as_list().size(), 12U);
}

TEST_F(WBERendGraphScriptVKResourceTest, RunsTheShippedDeferredRecipeWithItsVoxelAndPixelSortFeatures) {
    RenderAllocator allocator(WBE_KI_B(16));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    render_data.swap_chain_format = VK_FORMAT_B8G8R8A8_SRGB;
    auto make_extent = [](uint32_t p_width, uint32_t p_height) {
        DataValue extent = DataValue::make_map();
        extent.set_value("width", p_width);
        extent.set_value("height", p_height);
        return extent;
    };
    auto make_imports = [](std::initializer_list<const char*> p_names) {
        DataValue imports = DataValue::make_list();
        for (const char* name : p_names) {
            imports.push_back(DataValue::make_string(name));
        }
        return imports;
    };
    auto make_octree = [&] {
        DataValue octree = DataValue::make_map();
        octree.set_value("module", std::string("render_graphs.features.voxel_octree"));
        octree.set_entry("imports", make_imports({"octree_info", "octree_static", "octree_dynamic"}));
        octree.set_value("level_count", 2);
        DataValue leaf_groups = DataValue::make_map();
        leaf_groups.set_value("x", 4);
        leaf_groups.set_value("y", 4);
        leaf_groups.set_value("z", 2);
        octree.set_entry("leaf_gather_group_count", leaf_groups);
        octree.set_value("node_gather_group_count", 8);
        octree.set_value("leaf_draw_count", 33);
        return octree;
    };
    auto make_svo = [&] {
        DataValue svo = DataValue::make_map();
        svo.set_value("module", std::string("render_graphs.features.voxel_svo"));
        svo.set_value("variant", std::string("pass_published"));
        svo.set_value("shader_prefix", std::string("svo_2"));
        svo.set_value("dynamic_shader_prefix", std::string("svo_2_dynamic"));
        svo.set_entry("imports", make_imports({"svo_info", "svo_nodes"}));
        svo.set_value("extent", 512);
        svo.set_value("max_depth", 9);
        DataValue buffer_sizes = DataValue::make_map();
        buffer_sizes.set_value("dynamic_nodes", 1024);
        buffer_sizes.set_value("deferred_list", 516);
        buffer_sizes.set_value("pre_deferred_debug_info", 60);
        buffer_sizes.set_value("pre_deferred_debug_list", 512);
        svo.set_entry("buffer_sizes", buffer_sizes);
        for (const char* group_count : {
                 "reset_group_count",
                 "deferred_dispatch_group_count",
                 "dynamic_clear_group_count",
                 "static_publication_group_count",
                 "dynamic_publication_group_count",
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
        context.set_value("summed_area_table", p_summed_area_table);
        context.set_value("spot_light_count", 2);
        context.set_value("max_spot_light_info_count", 4);
        context.set_entry("shadow_extent", make_extent(256, 256));
        context.set_value("shadow_mipmap_level_count", 9);
        context.set_entry("bloom_extent", make_extent(640, 360));
        context.set_value("bloom_blur_pass_count", 4);
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
        context.set_value("frame_sequence_event", std::string("test.frames"));
        context.set_value("has_mesh_acceleration_structure", false);
        context.set_entry("voxel", p_voxel);
        DataValue transforms = DataValue::make_list();
        transforms.push_back(DataValue::make_string("dynamic_mesh_entity_0.transform"));
        transforms.push_back(DataValue::make_string("dynamic_mesh_entity_1.transform"));
        context.set_entry("dynamic_mesh_transform_ids", transforms);
        DataValue toggles = DataValue::make_map();
        for (const char* group : {"disabled", "enabled", "post_svo_build", "pre_deferred_propagation"}) {
            DataValue toggle = DataValue::make_map();
            toggle.set_value("event", std::string("test.voxel_debug_view.") + group);
            toggle.set_value("enabled", std::string_view(group) == "disabled");
            toggles.set_entry(group, toggle);
        }
        context.set_entry("voxel_debug_view_toggles", toggles);
        if (p_post_processing) {
            DataValue post_processing = DataValue::make_map();
            post_processing.set_value("module", std::string("render_graphs.features.pixel_sort"));
            post_processing.set_value("direction", std::string("vertical"));
            post_processing.set_value("scope", std::string("span"));
            post_processing.set_value("color_format", std::string("r16g16b16a16_sfloat"));
            post_processing.set_value("queue_family", std::string("compute_1"));
            post_processing.set_value("contrast_import", std::string("pixel_sort_contrast"));
            post_processing.set_value("toggle_event", std::string("test.pixel_sort"));
            post_processing.set_value("enabled", true);
            context.set_entry("post_processing", post_processing);
        }
        return RendGraphScriptVK::run("render_graphs.deferred", context);
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
    const auto find_by_id = [](const DataValue::List& p_entries, std::string_view p_id) -> const DataValue* {
        for (const DataValue& entry : p_entries) {
            if (entry.get_value<std::string>("id") == p_id) {
                return &entry;
            }
        }
        return nullptr;
    };
    // 2 lights x (4 shadow mesh draws + 1 filter), 4 G-buffer mesh draws, indirect lighting and its debug view, the frame
    // sequence acquisition, ImGui, direct and combine lighting, extract and blur bloom, the bloom acquisition, 2 presents,
    // 2 billboards, and the voxel subgraph.
    constexpr size_t BASE_TASK_COUNT = 10 + 4 + 2 + 1 + 1 + 2 + 2 + 1 + 2 + 2 + 1;
    const DataValue mipmap = run(false, false, make_octree());
    const DataValue::List& mipmap_tasks = mipmap.get_entry("tasks").as_list();
    EXPECT_EQ(mipmap_tasks.size(), BASE_TASK_COUNT);
    EXPECT_EQ(count_tasks_with(mipmap_tasks, "toggle"), 2U);
    EXPECT_EQ(count_tasks_with(mipmap_tasks, "trigger"), 2U);
    // The octree subgraph holds its frame-begin barrier, 6 imported C++ tasks, and 2 gathers and 2 overlays, all toggled,
    // and imports the application's textures and buffers.
    const DataValue* octree = find_by_id(mipmap_tasks, "voxel");
    ASSERT_NE(octree, nullptr);
    const DataValue& octree_recipe = octree->get_entry("subgraph");
    EXPECT_EQ(octree_recipe.get_entry("tasks").as_list().size(), 11U);
    EXPECT_EQ(count_tasks_with(octree_recipe.get_entry("tasks").as_list(), "toggle"), 4U);
    EXPECT_TRUE(contains_import(octree_recipe, "octree_dynamic"));
    EXPECT_EQ(octree->get_entry("ports").get_value<std::string>("depth"), "gbuffer_depth");
    // The pass-published SVO subgraph holds 15 tasks: the 5 static construction tasks are triggered, and the pre-deferred
    // capture and the 2 overlays toggled. The pixel sort subgraph holds 26 tasks, and its one toggle gates them all.
    const DataValue summed_area_table = run(true, true, make_svo());
    const DataValue::List& tasks = summed_area_table.get_entry("tasks").as_list();
    ASSERT_EQ(tasks.size(), BASE_TASK_COUNT + 1);
    EXPECT_EQ(count_tasks_with(tasks, "toggle"), 2U + 1U);
    const DataValue* svo = find_by_id(tasks, "voxel");
    ASSERT_NE(svo, nullptr);
    const DataValue& svo_recipe = svo->get_entry("subgraph");
    const DataValue::List& svo_tasks = svo_recipe.get_entry("tasks").as_list();
    EXPECT_EQ(svo_tasks.size(), 15U);
    EXPECT_EQ(count_tasks_with(svo_tasks, "toggle"), 3U);
    EXPECT_EQ(count_tasks_with(svo_tasks, "trigger"), 5U);
    EXPECT_TRUE(contains_import(svo_recipe, "svo_nodes"));
    // The SVO declares its 5 scratch buffers with the sizes it is given.
    const DataValue::List& svo_resources = svo_recipe.get_entry("resources").as_list();
    EXPECT_EQ(svo_resources.size(), 5U);
    const DataValue* deferred_list = find_by_id(svo_resources, "svo_deferred_list_ping");
    ASSERT_NE(deferred_list, nullptr);
    EXPECT_EQ(deferred_list->get_entry("buffer").get_value<int64_t>("size"), 516);
    const DataValue* dynamic_nodes = find_by_id(svo_resources, "svo_dynamic_nodes");
    ASSERT_NE(dynamic_nodes, nullptr);
    EXPECT_EQ(dynamic_nodes->get_entry("buffer").get_value<int64_t>("size"), 1024);
    const DataValue* pixel_sort = find_by_id(tasks, "post_processing");
    ASSERT_NE(pixel_sort, nullptr);
    EXPECT_TRUE(pixel_sort->contains("toggle"));
    const DataValue& pixel_sort_recipe = pixel_sort->get_entry("subgraph");
    EXPECT_EQ(pixel_sort_recipe.get_entry("tasks").as_list().size(), 26U);
    EXPECT_EQ(count_tasks_with(pixel_sort_recipe.get_entry("tasks").as_list(), "toggle"), 0U);
    EXPECT_TRUE(contains_import(pixel_sort_recipe, "pixel_sort_contrast"));
    EXPECT_EQ(pixel_sort->get_entry("ports").get_value<std::string>("color"), "result_image");
}

} // namespace WhiteBirdEngine
