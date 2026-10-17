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
#include "core/script/script_value.hh"
#include <cstdint>
#include <nlohmann/json.hpp>
#include "function/render/render_defs.hh"
#include "function/render_vk/rend_extent_2d_params_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_task_registry_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/rt_params_clear_value_vk.hh"
#include "function/render_vk/vulkan_enums_vk.hh"
#include "generated/enum_reflection.gen.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

// Writes the fixtures below as JSON for readability; the registry takes ScriptValue parameters.
ScriptValue to_script_value(const nlohmann::json& p_json) {
    if (p_json.is_object()) {
        ScriptValue result = ScriptValue::make_map();
        for (const auto& [key, value] : p_json.items()) {
            result.set_entry(key, to_script_value(value));
        }
        return result;
    }
    if (p_json.is_array()) {
        ScriptValue result = ScriptValue::make_list();
        for (const nlohmann::json& value : p_json) {
            result.push_back(to_script_value(value));
        }
        return result;
    }
    if (p_json.is_string()) {
        return ScriptValue::make_string(p_json.get<std::string>());
    }
    if (p_json.is_boolean()) {
        return ScriptValue::make_boolean(p_json.get<bool>());
    }
    if (p_json.is_number_integer()) {
        return ScriptValue::make_integer(p_json.get<int64_t>());
    }
    if (p_json.is_number_float()) {
        return ScriptValue::make_float(p_json.get<double>());
    }
    return ScriptValue();
}

ScriptValue parse_task_params(std::string_view p_json) {
    return to_script_value(nlohmann::json::parse(p_json));
}

struct BuiltinTaskCase {
    std::string_view type;
    std::string_view params;
};

// Valid parameters of every built-in task type, each with a pipeline key the render database does not hold.
constexpr BuiltinTaskCase BUILTIN_TASK_CASES[] = {
    {.type = "run_compute_pipeline", .params = R"({"pipeline": "p", "queue_family": "compute_1"})"},
    {.type = "run_compute_pipeline_indirect", .params = R"({"pipeline": "p", "indirect_buffer": "args", "indirect_offset": 16})"},
    {
        .type = "run_compute_pipelines_sequential",
        .params = R"({"dispatches": [{"pipeline": "a", "group_count_x": 4}, {"pipeline": "b"}], "queue_family": "COMPUTE_2"})",
    },
    {
        .type = "ping_pong",
        .params = R"({"ping_pipeline": "ping", "pong_pipeline": "pong", "iterations": 4, "ping_prefix_pipeline": "a", "pong_prefix_pipeline": "b"})",
    },
    {.type = "max_downsample", .params = R"({"pipeline": "p", "mip_views": ["mip.0", "mip.1"]})"},
    {
        .type = "run_graphics_pipeline",
        .params = R"({"pipeline": "p", "render_extent": {"swap_chain_scale": 0.5}, "clear_values": [{"color": [0, 0, 0, 1]}, {"depth": 1}],
                      "color_load_op": "load", "depth_load_op": "DONT_CARE", "draw_vertex_count": 6})",
    },
    {.type = "graphics_draw_mesh", .params = R"({"mesh": "sponza", "pipeline": "p", "parts": ["opaque", "masked"]})"},
};

} // namespace

TEST(WBERenderTaskRegistryVKBuiltinTest, RegistersEveryBuiltinTask) {
    for (std::string_view name : {
             "RTRunComputePipelineIndirectVK",
             "RTRunComputePipelinesSequentialVK",
             "RTPingPongVK",
             "RTMaxDownsampleVK",
             "RTGenerateMipmapsVK",
             "RTRunGraphicsPipelineVK",
             "run_compute_pipeline_indirect",
             "run_compute_pipelines_sequential",
             "ping_pong",
             "max_downsample",
             "generate_mipmaps",
             "run_graphics_pipeline",
         }) {
        EXPECT_TRUE(RenderTaskRegistryVK::contains(name)) << name;
    }
}

TEST(WBERenderTaskRegistryVKBuiltinTest, ValidParametersReachPipelineLookup) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    for (const BuiltinTaskCase& task_case : BUILTIN_TASK_CASES) {
        // Deserialization succeeds; creating the task fails only because no render database holds the pipelines.
        RendTaskFactoryVK factory;
        ASSERT_NO_THROW(factory = RenderTaskRegistryVK::create_factory(task_case.type, parse_task_params(task_case.params), &render_data))
            << task_case.type;
        try {
            (void)factory(RendGraphContextVK{.swap_chain_extent = {.width = 64, .height = 64}});
            ADD_FAILURE() << task_case.type << " created a task without a render database.";
        } catch (const std::runtime_error& p_error) {
            EXPECT_NE(std::string(p_error.what()).find("render database is unavailable"), std::string::npos)
                << task_case.type << ": " << p_error.what();
        }
    }
}

TEST(WBERenderTaskRegistryVKBuiltinTest, InvalidParametersFailWhenTheFactoryIsCreated) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    for (const BuiltinTaskCase& task_case : {
             BuiltinTaskCase{.type = "run_compute_pipeline_indirect", .params = R"({"pipeline": "p"})"},
             BuiltinTaskCase{.type = "run_compute_pipelines_sequential", .params = R"({"dispatches": [{"group_count_x": 1}]})"},
             BuiltinTaskCase{.type = "ping_pong", .params = R"({"ping_pipeline": "a", "pong_pipeline": "b", "iterations": 1, "iteration": 2})"},
             BuiltinTaskCase{.type = "max_downsample", .params = R"({"pipeline": "p", "mip_views": ["a"], "queue_family": "transfer"})"},
             BuiltinTaskCase{.type = "generate_mipmaps", .params = R"({"source_view": "a", "mip_chain_view": "b", "format": "rgba8"})"},
             BuiltinTaskCase{.type = "run_graphics_pipeline", .params = R"({"pipeline": "p", "color_load_op": "store"})"},
         }) {
        EXPECT_THROW((void)RenderTaskRegistryVK::create_factory(task_case.type, parse_task_params(task_case.params), &render_data), std::runtime_error)
            << task_case.type << ": " << task_case.params;
    }
}

TEST(WBERenderTaskRegistryVKBuiltinTest, CreateValidatesCrossFieldParameters) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    const RendGraphContextVK context{.swap_chain_extent = {.width = 64, .height = 64}};
    EXPECT_THROW((void)RenderTaskRegistryVK::create_factory("ping_pong",
                     parse_task_params(R"({"ping_pipeline": "a", "pong_pipeline": "b", "iterations": 1, "ping_prefix_pipeline": "c"})"),
                     &render_data)(context),
        std::runtime_error);
    EXPECT_THROW((void)RenderTaskRegistryVK::create_factory(
                     "run_compute_pipelines_sequential", parse_task_params(R"({"dispatches": []})"), &render_data)(context),
        std::runtime_error);
    // Rejected while resolving the extent, before the task is allocated.
    EXPECT_THROW((void)RenderTaskRegistryVK::create_factory("generate_mipmaps",
                     parse_task_params(R"({"source_view": "a", "mip_chain_view": "b", "format": "R8G8B8A8_UNORM", "base_extent": {"width": 4}})"),
                     &render_data)(context),
        std::runtime_error);
}

TEST(WBERendExtent2DParamsVKTest, ResolvesFixedAndSwapChainRelativeExtents) {
    const RendGraphContextVK context{.swap_chain_extent = {.width = 1280, .height = 721}};
    RendExtent2DParamsVK extent;
    EXPECT_EQ(extent.resolve(context).width, 1280U);
    EXPECT_EQ(extent.resolve(context).height, 721U);
    extent.swap_chain_scale = 0.5F;
    EXPECT_EQ(extent.resolve(context).width, 640U);
    EXPECT_EQ(extent.resolve(context).height, 360U);
    extent.swap_chain_scale = 0.0001F;
    EXPECT_EQ(extent.resolve(context).width, 1U);
    extent.width = 32;
    extent.height = 16;
    EXPECT_EQ(extent.resolve(context).width, 32U);
    EXPECT_EQ(extent.resolve(context).height, 16U);
}

TEST(WBERendExtent2DParamsVKTest, RejectsInvalidExtents) {
    const RendGraphContextVK context{.swap_chain_extent = {.width = 64, .height = 64}};
    EXPECT_THROW((void)RendExtent2DParamsVK{.width = 4}.resolve(context), std::runtime_error);
    EXPECT_THROW((void)(RendExtent2DParamsVK{.width = 0, .height = 4}.resolve(context)), std::runtime_error);
    EXPECT_THROW((void)(RendExtent2DParamsVK{.swap_chain_scale = -1.0F}.resolve(context)), std::runtime_error);
}

TEST(WBERTParamsClearValueVKTest, ConvertsColorAndDepth) {
    const VkClearValue color = RTParamsClearValueVK{.color = std::vector<float>{0.25F, 0.5F, 0.75F, 1.0F}}.to_vk();
    EXPECT_FLOAT_EQ(color.color.float32[2], 0.75F);
    const VkClearValue depth = RTParamsClearValueVK{.depth = 1.0F, .stencil = 3}.to_vk();
    EXPECT_FLOAT_EQ(depth.depthStencil.depth, 1.0F);
    EXPECT_EQ(depth.depthStencil.stencil, 3U);
    EXPECT_THROW((void)RTParamsClearValueVK{}.to_vk(), std::runtime_error);
    EXPECT_THROW((void)(RTParamsClearValueVK{.color = std::vector<float>{1.0F}, .depth = 1.0F}.to_vk()), std::runtime_error);
    EXPECT_THROW((void)(RTParamsClearValueVK{.color = std::vector<float>{1.0F, 1.0F, 1.0F}}.to_vk()), std::runtime_error);
}

TEST(WBEVulkanEnumMirrorTest, ValuesMatchVulkan) {
    EXPECT_EQ(to_vk_attachment_load_op(EnumReflection<AttachmentLoadOpVK>::string_to_enum("dont_care")), VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    EXPECT_EQ(to_vk_format(EnumReflection<ImageFormatVK>::string_to_enum("b8g8r8a8_srgb")), VK_FORMAT_B8G8R8A8_SRGB);
    EXPECT_EQ(to_vk_format(EnumReflection<ImageFormatVK>::string_to_enum("D32_SFLOAT")), VK_FORMAT_D32_SFLOAT);
}

} // namespace WhiteBirdEngine
