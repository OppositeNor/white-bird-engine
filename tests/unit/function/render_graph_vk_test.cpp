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
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/pipeline_descriptor_bindings_vk.hh"
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

constexpr std::string_view TEST_LOGICAL_RESOURCE = "logical";
constexpr std::string_view TEST_MISSING_RESOURCE = "missing";
constexpr std::string_view TEST_SHARED_RESOURCE = "shared";

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
    RTGraphResourceProbeVK(RenderAllocator* p_allocator, bool p_require_missing_resource, bool p_write_resource)
        : allocator(p_allocator), require_missing_resource(p_require_missing_resource), write_resource(p_write_resource) {
    }
    ~RTGraphResourceProbeVK() override = default;
    RTGraphResourceProbeVK(const RTGraphResourceProbeVK&) = delete;
    RTGraphResourceProbeVK(RTGraphResourceProbeVK&&) noexcept = delete;
    RTGraphResourceProbeVK& operator=(const RTGraphResourceProbeVK&) = delete;
    RTGraphResourceProbeVK& operator=(RTGraphResourceProbeVK&&) noexcept = delete;

    QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
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
    bool require_missing_resource;
    bool write_resource;
};

class RTGraphListAccessProbeVK final : public IRenderTaskVK {
public:
    RTGraphListAccessProbeVK(RenderAllocator* p_allocator, std::string_view p_resource_id, bool p_write)
        : allocator(p_allocator), resource_id(p_resource_id), write(p_write) {
    }
    ~RTGraphListAccessProbeVK() override = default;
    RTGraphListAccessProbeVK(const RTGraphListAccessProbeVK&) = delete;
    RTGraphListAccessProbeVK(RTGraphListAccessProbeVK&&) noexcept = delete;
    RTGraphListAccessProbeVK& operator=(const RTGraphListAccessProbeVK&) = delete;
    RTGraphListAccessProbeVK& operator=(RTGraphListAccessProbeVK&&) noexcept = delete;

    QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        Vector<ResAccessVK, RenderAllocator> result(allocator);
        if (!write) {
            result.push_back({.res_id = resource_id, .usage = RendResUsageVK::SAMPLED_IMAGE});
        }
        return result;
    }

    Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        Vector<ResAccessVK, RenderAllocator> result(allocator);
        if (write) {
            result.push_back({.res_id = resource_id, .usage = RendResUsageVK::STORAGE_IMAGE});
        }
        return result;
    }

    void record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) override {
    }

private:
    RenderAllocator* allocator;
    std::string resource_id;
    bool write;
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

TEST(WBERenderGraphVKTest, ResourceViewLooksUpGraphGlobalResources) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    Map<std::string, Ref<RendResVK>, RenderAllocator> resources(&allocator);
    Ref<ROCSingleVK> global_container = make_test_container(&allocator, &render_data);

    auto global_resource = make_ref<RendResVK>(&allocator);
    global_resource->res_id = TEST_SHARED_RESOURCE;
    global_resource->res_type_id = global_container->get_resource_type_id();
    global_resource->container = global_container;
    resources.emplace(std::string(TEST_SHARED_RESOURCE), global_resource);

    RendResViewVK resource_view(&resources);
    EXPECT_EQ(resource_view.get(TEST_SHARED_RESOURCE).container, global_container);
    EXPECT_TRUE(resource_view.contains(TEST_SHARED_RESOURCE));
    EXPECT_THROW(resource_view.get(TEST_MISSING_RESOURCE), std::runtime_error);
}

TEST(WBERenderGraphVKTest, RejectsDuplicateGraphResourceRegistration) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    RendGraphDescVK descriptor(&allocator);
    descriptor.add_resource(TEST_LOGICAL_RESOURCE, make_test_container(&allocator, &render_data));
    descriptor.add_resource(TEST_LOGICAL_RESOURCE, make_test_container(&allocator, &render_data));
    RenderGraphVK graph(&render_data);

    EXPECT_THROW(graph.build_from_descriptor(descriptor), std::runtime_error);
}

TEST(WBERenderGraphVKTest, MissingReflectedResourceFailsDuringTaskBinding) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    RendGraphDescVK descriptor(&allocator);
    descriptor.add_task("missing", make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, true, false));
    RenderGraphVK graph(&render_data);

    EXPECT_THROW(graph.build_from_descriptor(descriptor), std::runtime_error);
}

TEST(WBERenderGraphVKTest, GlobalResourceAccessesConflictAcrossTasks) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    RendGraphDescVK descriptor(&allocator);
    descriptor.add_resource(TEST_LOGICAL_RESOURCE, make_test_container(&allocator, &render_data));
    descriptor.add_task("first", make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, false, true));
    descriptor.add_task("second", make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, false, true));
    RenderGraphVK graph(&render_data);

    try {
        graph.build_from_descriptor(descriptor);
        FAIL() << "Expected conflicting graph-global resource accesses to fail.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("resource 'logical' without an explicit execution path"), std::string::npos);
    }
}

TEST(WBERenderGraphVKTest, ResourceListAccessExpandsToIndexedResourcesForConflictValidation) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    auto resource_list = make_ref<RenderResourceList>(&allocator, &allocator);
    resource_list->add_resource(
        make_ref<TestDescriptorResourceVK>(&allocator, &render_data, DebugDataArray{}).dynamic_cast_ref<IRenderObject>());
    RendGraphDescVK descriptor(&allocator);
    descriptor.add_resource("textures", resource_list);
    descriptor.add_task("writer", make_unique<RTGraphListAccessProbeVK>(&allocator, &allocator, "textures", true));
    descriptor.add_task("reader", make_unique<RTGraphListAccessProbeVK>(&allocator, &allocator, "textures", false));
    RenderGraphVK graph(&render_data);

    try {
        graph.build_from_descriptor(descriptor);
        FAIL() << "Expected conflicting expanded resource-list accesses to fail.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("tasks '"), std::string::npos);
        EXPECT_NE(message.find("writer"), std::string::npos);
        EXPECT_NE(message.find("reader"), std::string::npos);
        EXPECT_NE(message.find("resource 'textures[0]' without an explicit execution path"), std::string::npos);
    }
}

TEST(WBERenderGraphVKTest, ExplicitMappedResourceAccessesStillRequireExecutionEdges) {
    RenderAllocator allocator(WBE_KI_B(128));
    BaseRenderDataVK render_data(&allocator);
    PipelineResourceMapVK mapping(&allocator);
    mapping.bind("static_nodes", "dynamic_nodes");
    ShaderDescriptorHeapLayoutVK layout;
    layout.size = 8;
    layout.fields = {
        {.field_path = "nodes", .type = ShaderDescriptorHeapTypeVK::STORAGE_BUFFER, .resource_id = "static_nodes", .offset = 0}};
    PipelineDescriptorBindingsVK invocation(&render_data, layout, mapping);
    RendGraphDescVK descriptor(&allocator);
    descriptor.add_resource("dynamic_nodes", make_test_container(&allocator, &render_data));
    RendTaskDescVK writer(&allocator, "writer", make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, false, false));
    writer.write("dynamic_nodes", RendResUsageVK::STORAGE_BUFFER);
    RendTaskDescVK reader(&allocator, "mapped_reader", make_unique<RTGraphResourceProbeVK>(&allocator, &allocator, false, false));
    reader.read("dynamic_nodes", RendResUsageVK::STORAGE_BUFFER);
    descriptor.add_task(std::move(writer)).add_task(std::move(reader));
    RenderGraphVK graph(&render_data);
    try {
        graph.build_from_descriptor(descriptor);
        FAIL() << "A mapping must not infer an execution edge.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("resource 'dynamic_nodes' without an explicit execution path"), std::string::npos);
    }
}

} // namespace WhiteBirdEngine
