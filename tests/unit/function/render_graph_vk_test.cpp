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
#include "core/memory/unique.hh"
#include "core/parser/parser_json.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

static constexpr std::string_view TEST_LOGICAL_RESOURCE = "logical";
static constexpr std::string_view TEST_MISSING_RESOURCE = "missing";
static constexpr std::string_view TEST_SHARED_RESOURCE = "shared";
static constexpr std::string_view TEST_LIGHTING_TASK = "lighting";
static constexpr std::string_view TEST_LIGHTING_SHARED_RESOURCE = "lighting.shared";

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

Ref<ROCSingleVK> make_test_container(RenderAllocator* p_allocator, const BaseRenderDataVK* p_render_data) {
    return make_ref<ROCSingleVK>(p_allocator,
        make_ref<TestDescriptorResourceVK>(p_allocator, p_render_data, DebugDataArray{}).dynamic_cast_ref<IRenderObjectVK>());
}

class RTGraphResourceProbeVK final : public IRenderTaskVK {
public:
    RTGraphResourceProbeVK(
        RenderAllocator* p_allocator, Ref<ROCSingleVK> p_container, size_t p_registration_count, bool p_require_missing_resource, bool p_write_resource)
        : allocator(p_allocator), container(std::move(p_container)), registration_count(p_registration_count),
          require_missing_resource(p_require_missing_resource), write_resource(p_write_resource) {
    }
    ~RTGraphResourceProbeVK() override = default;
    RTGraphResourceProbeVK(const RTGraphResourceProbeVK&) = delete;
    RTGraphResourceProbeVK(RTGraphResourceProbeVK&&) noexcept = delete;
    RTGraphResourceProbeVK& operator=(const RTGraphResourceProbeVK&) = delete;
    RTGraphResourceProbeVK& operator=(RTGraphResourceProbeVK&&) noexcept = delete;

    QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    void register_resources(RendResRegistrarVK& p_registrar) override {
        for (size_t i = 0; i < registration_count; ++i) {
            p_registrar.add_resource(TEST_LOGICAL_RESOURCE, container->get_resource_type_id(), container);
        }
    }

    void bind_resources(const RendResViewVK& p_resources) override {
        if (require_missing_resource) {
            static_cast<void>(p_resources.get(TEST_MISSING_RESOURCE));
        }
    }

    Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        return Vector<ResAccessVK, RenderAllocator>(allocator);
    }

    Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        Vector<ResAccessVK, RenderAllocator> result(allocator);
        if (write_resource) {
            result.push_back({.res_id = std::string(TEST_LOGICAL_RESOURCE), .usage = RendResUsageVK::STORAGE_BUFFER});
        }
        return result;
    }

    void record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) override {
    }

private:
    RenderAllocator* allocator;
    Ref<ROCSingleVK> container;
    size_t registration_count;
    bool require_missing_resource;
    bool write_resource;
};

} // namespace

TEST(WBERenderGraphVKTest, RendResViewDescriptorDebugDataAddsResourceIdsAndSortsDeterministically) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    render_data.frames_in_flight = 2;
    Map<std::string, Ref<RendResVK>, RenderAllocator> resources(&allocator);

    auto single_container = make_ref<ROCSingleVK>(&allocator,
        make_ref<TestDescriptorResourceVK>(
            &allocator, &render_data, DebugDataArray{make_descriptor_row("ROImageViewVK", "sampled_image", "resource", 7)})
            .template dynamic_cast_ref<IRenderObjectVK>());
    auto per_frame_container = make_ref<ROCMultislotsVK>(&allocator, &allocator, render_data.frames_in_flight, false);
    per_frame_container->set(0,
        make_ref<TestDescriptorResourceVK>(
            &allocator, &render_data, DebugDataArray{make_descriptor_row("ROSamplerVK", "sampler", "sampler", 7)})
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
    EXPECT_EQ(rows[0].get_value<uint32_t>("slot_index"), 1U);
    EXPECT_EQ(rows[1].get_value<uint32_t>("heap_index"), 7U);
    EXPECT_EQ(rows[1].get_value<std::string>("heap"), "resource");
    EXPECT_EQ(rows[1].get_value<std::string>("resource_id"), "alpha");
    EXPECT_FALSE(rows[1].contains("slot_index"));
    EXPECT_EQ(rows[2].get_value<uint32_t>("heap_index"), 7U);
    EXPECT_EQ(rows[2].get_value<std::string>("heap"), "resource");
    EXPECT_EQ(rows[2].get_value<std::string>("resource_id"), "beta");
    EXPECT_EQ(rows[2].get_value<uint32_t>("slot_index"), 1U);
    EXPECT_EQ(rows[3].get_value<std::string>("heap"), "sampler");
    EXPECT_EQ(rows[3].get_value<std::string>("resource_id"), "beta");
    EXPECT_EQ(rows[3].get_value<uint32_t>("slot_index"), 0U);
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

TEST(WBERenderGraphVKTest, TaskResourceViewPrefersQualifiedResourcesAndFallsBackToGlobals) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    Map<std::string, Ref<RendResVK>, RenderAllocator> resources(&allocator);
    Ref<ROCSingleVK> global_container = make_test_container(&allocator, &render_data);
    Ref<ROCSingleVK> local_container = make_test_container(&allocator, &render_data);

    auto global_resource = make_ref<RendResVK>(&allocator);
    global_resource->res_id = TEST_SHARED_RESOURCE;
    global_resource->res_type_id = global_container->get_resource_type_id();
    global_resource->container = global_container;
    resources.emplace(std::string(TEST_SHARED_RESOURCE), global_resource);
    auto local_resource = make_ref<RendResVK>(&allocator);
    local_resource->res_id = TEST_LIGHTING_SHARED_RESOURCE;
    local_resource->res_type_id = local_container->get_resource_type_id();
    local_resource->container = local_container;
    resources.emplace(std::string(TEST_LIGHTING_SHARED_RESOURCE), local_resource);

    RendResViewVK lighting_view(&resources, TEST_LIGHTING_TASK);
    RendResViewVK other_view(&resources, "other");
    EXPECT_EQ(lighting_view.get(TEST_SHARED_RESOURCE).container, local_container);
    EXPECT_EQ(other_view.get(TEST_SHARED_RESOURCE).container, global_container);
    EXPECT_TRUE(lighting_view.contains(TEST_SHARED_RESOURCE));
    EXPECT_THROW(lighting_view.get(TEST_MISSING_RESOURCE), std::runtime_error);
}

TEST(WBERenderGraphVKTest, RejectsDuplicateTaskPrivateResourceRegistration) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    RendGraphDescVK descriptor(&allocator);
    descriptor.add_task("duplicate",
        make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, make_test_container(&allocator, &render_data), 2, false, false));
    RenderGraphVK graph(&render_data);

    EXPECT_THROW(graph.build_from_descriptor(descriptor), std::runtime_error);
}

TEST(WBERenderGraphVKTest, MissingReflectedResourceFailsDuringTaskBinding) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    RendGraphDescVK descriptor(&allocator);
    descriptor.add_task("missing",
        make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, make_test_container(&allocator, &render_data), 0, true, false));
    RenderGraphVK graph(&render_data);

    EXPECT_THROW(graph.build_from_descriptor(descriptor), std::runtime_error);
}

TEST(WBERenderGraphVKTest, NormalizesSameLogicalTaskResourcesBeforeConflictValidation) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    RendGraphDescVK descriptor(&allocator);
    descriptor.add_task("first",
        make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, make_test_container(&allocator, &render_data), 1, false, true));
    descriptor.add_task("second",
        make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, make_test_container(&allocator, &render_data), 1, false, true));
    RenderGraphVK graph(&render_data);

    try {
        graph.build_from_descriptor(descriptor);
        FAIL() << "Expected graph construction to reach command-pool allocation.";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "Failed to create command pool for queue family: 0.");
    }
}

} // namespace WhiteBirdEngine
