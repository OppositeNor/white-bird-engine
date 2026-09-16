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
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/parser/parser_json.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/roc_per_frame_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

namespace {

DebugData make_descriptor_row(
    std::string_view p_resource_type, std::string_view p_descriptor_type, std::string_view p_heap, uint32_t p_heap_index) {
    DebugData row;
    row.set_value("resource_type", std::string(p_resource_type));
    row.set_value("descriptor_type", std::string(p_descriptor_type));
    row.set_value("heap", std::string(p_heap));
    row.set_value("heap_index", p_heap_index);
    row.set_value("device_address", nullptr);
    return row;
}

class TestDescriptorResourceVK final : public IRenderObjectVK {
public:
    TestDescriptorResourceVK(const BaseRenderDataVK* p_render_data, DebugDataArray p_rows)
        : IRenderObjectVK(p_render_data), rows(p_render_data->allocator) {
        rows.reserve(p_rows.size());
        for (DebugData& row : p_rows) {
            rows.emplace_back(std::move(row));
        }
    }
    virtual ~TestDescriptorResourceVK() override = default;
    TestDescriptorResourceVK(const TestDescriptorResourceVK&) = delete;
    TestDescriptorResourceVK(TestDescriptorResourceVK&&) noexcept = delete;
    TestDescriptorResourceVK& operator=(const TestDescriptorResourceVK&) = delete;
    TestDescriptorResourceVK& operator=(TestDescriptorResourceVK&&) noexcept = delete;

    TypeID get_type_uuid() const override {
        return static_hash("RenderGraphTestDescriptorResourceVK");
    }

    DebugDataRows descriptor_debug_data() const override {
        return rows;
    }

private:
    DebugDataRows rows;
};

} // namespace

TEST(WBERenderGraphVKTest, RendResViewDescriptorDebugDataAddsResourceIdsAndSortsDeterministically) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    Map<std::string, Ref<RendResVK>, RenderAllocator> resources(&allocator);

    auto single_container = make_ref<ROCSingleVK>(&allocator,
        make_ref<TestDescriptorResourceVK>(&allocator,
            &render_data,
            DebugDataArray{make_descriptor_row("ROImageViewVK", "sampled_image", "resource", 7)})
            .template dynamic_cast_ref<IRenderObjectVK>());
    auto per_frame_container = make_ref<ROCPerFrameVK>(&allocator, &render_data, false);
    per_frame_container->set(0,
        make_ref<TestDescriptorResourceVK>(&allocator,
            &render_data,
            DebugDataArray{make_descriptor_row("ROSamplerVK", "sampler", "sampler", 7)})
            .template dynamic_cast_ref<IRenderObjectVK>());
    per_frame_container->set(1,
        make_ref<TestDescriptorResourceVK>(&allocator,
            &render_data,
            DebugDataArray{make_descriptor_row("ROBufferVK", "uniform_buffer", "resource", 3),
                make_descriptor_row("ROBufferVK", "storage_buffer", "resource", 7)})
            .template dynamic_cast_ref<IRenderObjectVK>());

    auto alpha_resource = make_ref<RendResVK>(&allocator);
    alpha_resource->res_id = "alpha";
    alpha_resource->res_type_id = single_container->get_resource_type_id();
    alpha_resource->container = single_container;
    resources.emplace("alpha", alpha_resource);

    auto beta_resource = make_ref<RendResVK>(&allocator);
    beta_resource->res_id = "beta";
    beta_resource->res_type_id = per_frame_container->get_resource_type_id();
    beta_resource->container = per_frame_container;
    resources.emplace("beta", beta_resource);

    RendResViewVK resource_view(&resources);
    DebugDataRows rows = resource_view.descriptor_debug_data();

    ASSERT_EQ(rows.size(), 4U);
    EXPECT_EQ(rows[0].get_value<uint32_t>("heap_index"), 3U);
    EXPECT_EQ(rows[0].get_value<std::string>("resource_id"), "beta");
    EXPECT_EQ(rows[0].get_value<uint32_t>("frame_index"), 1U);
    EXPECT_EQ(rows[1].get_value<uint32_t>("heap_index"), 7U);
    EXPECT_EQ(rows[1].get_value<std::string>("heap"), "resource");
    EXPECT_EQ(rows[1].get_value<std::string>("resource_id"), "alpha");
    EXPECT_FALSE(rows[1].contains("frame_index"));
    EXPECT_EQ(rows[2].get_value<uint32_t>("heap_index"), 7U);
    EXPECT_EQ(rows[2].get_value<std::string>("heap"), "resource");
    EXPECT_EQ(rows[2].get_value<std::string>("resource_id"), "beta");
    EXPECT_EQ(rows[2].get_value<uint32_t>("frame_index"), 1U);
    EXPECT_EQ(rows[3].get_value<std::string>("heap"), "sampler");
    EXPECT_EQ(rows[3].get_value<std::string>("resource_id"), "beta");
    EXPECT_EQ(rows[3].get_value<uint32_t>("frame_index"), 0U);
}

TEST(WBERenderGraphVKTest, ListDescriptorsReturnsEmptyJsonForEmptyGraph) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 1;
    RenderGraphVK graph(&render_data);

    DebugData descriptor_data = graph.list_descriptors();
    DebugDataArray rows = descriptor_data.get<DebugDataArray>();

    EXPECT_TRUE(rows.empty());
}

} // namespace WhiteBirdEngine