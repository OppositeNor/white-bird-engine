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
#include "core/alloc/heap_alloc.hh"
#include "core/alloc/i_alloc.hh"
#include "core/alloc/stack_alloc.hh"
#include "core/alloc/stl_alloc.hh"
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/parser/parser_json.hh"
#include "function/render/i_rend_graph.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/rend_defs.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/rend_vk/debug_data_vk.hh"
#include "function/rend_vk/i_rend_graph_bindings_vk.hh"
#include "function/rend_vk/i_rend_object_vk.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/rd_params_compute_pipeline_vk.hh"
#include "function/rend_vk/rd_params_graphics_pipeline_vk.hh"
#include "function/rend_vk/rd_params_image_vk.hh"
#include "function/rend_vk/rd_params_mesh_vk.hh"
#include "function/rend_vk/rend_recipe_context_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "function/rend_vk/rend_graph_metadata_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/rend_vk/rend_object_container_vk.hh"
#include "function/rend_vk/rend_resource_list_vk.hh"
#include "function/rend_vk/roc_multislots_vk.hh"
#include "function/rend_vk/roc_single_vk.hh"
#include "function/rend_vk/rt_triggered_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "global/global.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "test_global.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <new>
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

DebugData make_desc_row(
    std::string_view p_resource_type, std::string_view p_desc_type, std::string_view p_heap, uint32_t p_heap_index) {
    DebugData row;
    row.set_value("resource_type", std::string(p_resource_type));
    row.set_value("desc_type", std::string(p_desc_type));
    row.set_value("heap", std::string(p_heap));
    row.set_value("heap_index", p_heap_index);
    row.set_value("device_address", nullptr);
    return row;
}

class TestDescResourceVK final : public IRendObjectVK {
public:
    TestDescResourceVK(const RendContextVK* p_rend_data, DebugDataArray p_rows)
        : IRendObjectVK(p_rend_data), rows(p_rend_data->alloc) {
        rows.reserve(p_rows.size());
        for (DebugData& row : p_rows) {
            rows.emplace_back(std::move(row));
        }
    }
    virtual ~TestDescResourceVK() override = default;
    TestDescResourceVK(const TestDescResourceVK&) = delete;
    TestDescResourceVK(TestDescResourceVK&&) noexcept = delete;
    TestDescResourceVK& operator=(const TestDescResourceVK&) = delete;
    TestDescResourceVK& operator=(TestDescResourceVK&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return static_hash("RendGraphTestDescResourceVK");
    }

    virtual DebugDataRows desc_debug_data() const override {
        return rows;
    }

private:
    DebugDataRows rows;
};

Ref<ROCSingleVK> make_test_container(RendAlloc* p_alloc, const RendContextVK* p_rend_data) {
    Ref<IRendObjectVK> resource =
        make_ref<TestDescResourceVK>(p_alloc, p_rend_data, DebugDataArray{}).dynamic_cast_ref<IRendObjectVK>();
    return make_ref<ROCSingleVK>(p_alloc, [resource](uint32_t /*p_slot*/) {
        return resource;
    });
}

class RTGraphResourceProbeVK final : public IRendTaskVK {
public:
    RTGraphResourceProbeVK(bool p_require_missing_resource, bool p_write_resource)
        : require_missing_resource(p_require_missing_resource), write_resource(p_write_resource) {
    }
    virtual ~RTGraphResourceProbeVK() override = default;
    RTGraphResourceProbeVK(const RTGraphResourceProbeVK&) = delete;
    RTGraphResourceProbeVK(RTGraphResourceProbeVK&&) noexcept = delete;
    RTGraphResourceProbeVK& operator=(const RTGraphResourceProbeVK&) = delete;
    RTGraphResourceProbeVK& operator=(RTGraphResourceProbeVK&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void bind_resources(const RendResViewVK& p_resources) override {
        if (require_missing_resource) {
            static_cast<void>(p_resources.get(TEST_MISSING_RESOURCE));
        }
    }

    virtual void append_accesses(
        Vector<ResAccessVK, RendAlloc>& /*p_reads*/, Vector<ResAccessVK, RendAlloc>& p_writes) const override {
        if (write_resource) {
            p_writes.push_back({.res_id = std::string(TEST_LOGICAL_RESOURCE), .usage = RendResUsageVK::STORAGE_BUFFER});
        }
    }

    virtual void record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) override {
    }

private:
    bool require_missing_resource;
    bool write_resource;
};

class RTGraphListAccessProbeVK final : public IRendTaskVK {
public:
    RTGraphListAccessProbeVK(std::string_view p_resource_id, bool p_write) : resource_id(p_resource_id), write(p_write) {
    }
    virtual ~RTGraphListAccessProbeVK() override = default;
    RTGraphListAccessProbeVK(const RTGraphListAccessProbeVK&) = delete;
    RTGraphListAccessProbeVK(RTGraphListAccessProbeVK&&) noexcept = delete;
    RTGraphListAccessProbeVK& operator=(const RTGraphListAccessProbeVK&) = delete;
    RTGraphListAccessProbeVK& operator=(RTGraphListAccessProbeVK&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void append_accesses(Vector<ResAccessVK, RendAlloc>& p_reads, Vector<ResAccessVK, RendAlloc>& p_writes) const override {
        if (write) {
            p_writes.push_back({.res_id = resource_id, .usage = RendResUsageVK::STORAGE_IMAGE});
        } else {
            p_reads.push_back({.res_id = resource_id, .usage = RendResUsageVK::SAMPLED_IMAGE});
        }
    }

    virtual void record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) override {
    }

private:
    std::string resource_id;
    bool write;
};

class RTGraphEdgeProbeVK final : public IRendTaskVK {
public:
    RTGraphEdgeProbeVK(RendAlloc* p_alloc,
        QueueFamilyVK p_queue_family,
        const std::vector<std::string>& p_reads,
        const std::vector<std::string>& p_writes)
        : queue(p_queue_family), reads(p_alloc), writes(p_alloc) {
        for (const std::string& read : p_reads) {
            reads.push_back({.res_id = read, .usage = RendResUsageVK::STORAGE_BUFFER});
        }
        for (const std::string& write : p_writes) {
            writes.push_back({.res_id = write, .usage = RendResUsageVK::STORAGE_BUFFER});
        }
    }
    virtual ~RTGraphEdgeProbeVK() override = default;
    RTGraphEdgeProbeVK(const RTGraphEdgeProbeVK&) = delete;
    RTGraphEdgeProbeVK(RTGraphEdgeProbeVK&&) noexcept = delete;
    RTGraphEdgeProbeVK& operator=(const RTGraphEdgeProbeVK&) = delete;
    RTGraphEdgeProbeVK& operator=(RTGraphEdgeProbeVK&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return queue;
    }

    virtual void append_accesses(Vector<ResAccessVK, RendAlloc>& p_reads, Vector<ResAccessVK, RendAlloc>& p_writes) const override {
        p_reads.insert(p_reads.end(), reads.begin(), reads.end());
        p_writes.insert(p_writes.end(), writes.begin(), writes.end());
    }

    virtual void record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) override {
    }

private:
    QueueFamilyVK queue;
    Vector<ResAccessVK, RendAlloc> reads;
    Vector<ResAccessVK, RendAlloc> writes;
};

// Graph validation succeeded when construction reaches command-buffer allocation, which needs a Vulkan device.
constexpr std::string_view TEST_VALIDATION_PASSED_MESSAGE = "Failed to create command pool";

RendTaskDescVK make_probe_desc(RendAlloc* p_alloc,
    std::string_view p_task_id,
    QueueFamilyVK p_queue_family,
    std::initializer_list<std::string_view> p_reads,
    std::initializer_list<std::string_view> p_writes) {
    return RendTaskDescVK(p_alloc,
        p_task_id,
        [p_alloc,
            p_queue_family,
            reads = std::vector<std::string>(p_reads.begin(), p_reads.end()),
            writes = std::vector<std::string>(p_writes.begin(), p_writes.end())](const RendGraphContextVK& /*p_context*/) {
            return make_unique<RTGraphEdgeProbeVK>(p_alloc, p_alloc, p_queue_family, reads, writes);
        });
}

// Build p_graph from a copy of p_desc.
void build_graph(RendGraphVK& p_graph, const RendGraphDescVK& p_desc) {
    p_graph.build_from_desc(p_desc);
}

std::string build_error(RendGraphVK& p_graph, RendGraphDescVK& p_desc) {
    try {
        build_graph(p_graph, p_desc);
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

} // namespace

TEST(WBERendGraphVKTest, RendResViewDescDebugDataAddsResourceIdsAndSortsDeterministically) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    rend_data.frames_in_flight = 2;
    Map<std::string, Ref<RendResVK>, RendAlloc> resources(&alloc);

    Ref<IRendObjectVK> single_resource = make_ref<TestDescResourceVK>(
        &alloc, &rend_data, DebugDataArray{make_desc_row("ROImageViewVK", "sampled_image", "resource", 7)})
                                               .template dynamic_cast_ref<IRendObjectVK>();
    auto single_container = make_ref<ROCSingleVK>(&alloc, [single_resource](uint32_t /*p_slot*/) {
        return single_resource;
    });
    auto per_frame_container = make_ref<ROCMultislotsVK>(
        &alloc,
        &alloc,
        rend_data.frames_in_flight,
        [&alloc, &rend_data](uint32_t /*p_slot*/) {
            return make_ref<TestDescResourceVK>(&alloc, &rend_data, DebugDataArray{}).template dynamic_cast_ref<IRendObjectVK>();
        },
        false);
    per_frame_container->set(0,
        make_ref<TestDescResourceVK>(
            &alloc, &rend_data, DebugDataArray{make_desc_row("ROSamplerVK", "sampler", "sampler", 7)})
            .template dynamic_cast_ref<IRendObjectVK>());
    per_frame_container->set(1,
        make_ref<TestDescResourceVK>(&alloc,
            &rend_data,
            DebugDataArray{make_desc_row("ROBufferVK", "uniform_buffer", "resource", 3),
                make_desc_row("ROBufferVK", "storage_buffer", "resource", 7)})
            .template dynamic_cast_ref<IRendObjectVK>());

    auto alpha_resource = make_ref<RendResVK>(&alloc);
    alpha_resource->res_id = "alpha";
    alpha_resource->res_type_id = single_container->get_resource_type_id();
    alpha_resource->container = single_container;
    resources.emplace("alpha", alpha_resource);

    auto beta_resource = make_ref<RendResVK>(&alloc);
    beta_resource->res_id = "beta";
    beta_resource->res_type_id = per_frame_container->get_resource_type_id();
    beta_resource->container = per_frame_container;
    resources.emplace("beta", beta_resource);

    RendResViewVK resource_view(&resources);
    DebugDataRows rows = resource_view.desc_debug_data();

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

TEST(WBERendGraphVKTest, ListDescsReturnsEmptyJsonForEmptyGraph) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    rend_data.frames_in_flight = 1;
    RendGraphVK graph(&rend_data);

    DebugData desc_data = graph.list_descs();
    DebugDataArray rows = desc_data.get<DebugDataArray>();

    EXPECT_TRUE(rows.empty());
}

TEST(WBERendGraphVKTest, ResourceViewLooksUpGraphGlobalResources) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    Map<std::string, Ref<RendResVK>, RendAlloc> resources(&alloc);
    Ref<ROCSingleVK> global_container = make_test_container(&alloc, &rend_data);

    auto global_resource = make_ref<RendResVK>(&alloc);
    global_resource->res_id = TEST_SHARED_RESOURCE;
    global_resource->res_type_id = global_container->get_resource_type_id();
    global_resource->container = global_container;
    resources.emplace(std::string(TEST_SHARED_RESOURCE), global_resource);

    RendResViewVK resource_view(&resources);
    EXPECT_EQ(resource_view.get(TEST_SHARED_RESOURCE).container, global_container);
    EXPECT_TRUE(resource_view.contains(TEST_SHARED_RESOURCE));
    EXPECT_THROW(resource_view.get(TEST_MISSING_RESOURCE), std::runtime_error);
}

TEST(WBERendGraphVKTest, RejectsDuplicateGraphResourceRegistration) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    RendGraphVK graph(&rend_data);

    EXPECT_THROW(build_graph(graph, desc), std::runtime_error);
}

TEST(WBERendGraphVKTest, AcquiresExportedResourcesByTheirExportNames) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data)).add_export("exported", TEST_LOGICAL_RESOURCE);
    desc.add_task(make_probe_desc(&alloc, "probe", QueueFamilyVK::GRAPHICS, {TEST_LOGICAL_RESOURCE}, {}));
    RendGraphVK graph(&rend_data);
    EXPECT_NE(build_error(graph, desc).find(TEST_VALIDATION_PASSED_MESSAGE), std::string::npos);
    EXPECT_NE(graph.acquire_resource("exported"), MEM_NULL);
    // C++ reaches resources only through their export names.
    EXPECT_THROW((void)graph.acquire_resource(TEST_LOGICAL_RESOURCE), std::runtime_error);

    RendGraphDescVK undeclared(&alloc);
    undeclared.add_export("exported", TEST_MISSING_RESOURCE);
    undeclared.add_task(make_probe_desc(&alloc, "probe", QueueFamilyVK::GRAPHICS, {}, {}));
    RendGraphVK undeclared_graph(&rend_data);
    EXPECT_NE(build_error(undeclared_graph, undeclared).find("export 'exported' names resource"), std::string::npos);
}

TEST(WBERendGraphVKTest, MissingReflectedResourceFailsDuringTaskBinding) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_task("missing", [&alloc](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTGraphResourceProbeVK>(&alloc, true, false);
    });
    RendGraphVK graph(&rend_data);

    EXPECT_THROW(build_graph(graph, desc), std::runtime_error);
}

TEST(WBERendGraphVKTest, GlobalResourceAccessesConflictAcrossTasks) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_task("first", [&alloc](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTGraphResourceProbeVK>(&alloc, false, true);
    });
    desc.add_task("second", [&alloc](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTGraphResourceProbeVK>(&alloc, false, true);
    });
    RendGraphVK graph(&rend_data);

    try {
        build_graph(graph, desc);
        FAIL() << "Expected conflicting graph-global resource accesses to fail.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("resource 'logical' without a chain of edges on that resource"), std::string::npos);
    }
}

TEST(WBERendGraphVKTest, InactiveTriggeredWritesStillRequireExplicitOrdering) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_task("first", [&alloc](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTTriggeredVK>(&alloc, make_unique<RTGraphResourceProbeVK>(&alloc, false, true));
    });
    desc.add_task("second", [&alloc](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTTriggeredVK>(&alloc, make_unique<RTGraphResourceProbeVK>(&alloc, false, true));
    });
    RendGraphVK graph(&rend_data);

    try {
        build_graph(graph, desc);
        FAIL() << "Expected conflicting graph-global resource accesses to fail.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("resource 'logical' without a chain of edges on that resource"), std::string::npos);
    }
}

TEST(WBERendGraphVKTest, ResourceListAccessExpandsToIndexedResourcesForConflictValidation) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    auto resource_list = make_ref<RendResourceListVK>(&alloc, &alloc);
    resource_list->add_resource(
        make_ref<TestDescResourceVK>(&alloc, &rend_data, DebugDataArray{}).dynamic_cast_ref<IRendObject>());
    RendGraphDescVK desc(&alloc);
    desc.add_resource("textures", *resource_list);
    desc.add_task("writer", [&alloc](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTGraphListAccessProbeVK>(&alloc, "textures", true);
    });
    desc.add_task("reader", [&alloc](const RendGraphContextVK& /*p_context*/) {
        return make_unique<RTGraphListAccessProbeVK>(&alloc, "textures", false);
    });
    RendGraphVK graph(&rend_data);

    try {
        build_graph(graph, desc);
        FAIL() << "Expected conflicting expanded resource-list accesses to fail.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("tasks '"), std::string::npos);
        EXPECT_NE(message.find("writer"), std::string::npos);
        EXPECT_NE(message.find("reader"), std::string::npos);
        EXPECT_NE(message.find("resource 'textures[0]' without a chain of edges on that resource"), std::string::npos);
    }
}

TEST(WBERendGraphVKTest, AliasedTaskAccessesResolveToGraphResourcesForValidation) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource("dynamic_nodes", *make_test_container(&alloc, &rend_data));
    // Both tasks access the task-local "nodes" identifier, which their descriptors map to the same graph resource.
    RendTaskDescVK writer = make_probe_desc(&alloc, "writer", QueueFamilyVK::GRAPHICS, {}, {"nodes"});
    writer.alias("nodes", "dynamic_nodes");
    RendTaskDescVK reader = make_probe_desc(&alloc, "mapped_reader", QueueFamilyVK::GRAPHICS, {"nodes"}, {});
    reader.alias("nodes", "dynamic_nodes");
    desc.add_task(std::move(writer)).add_task(std::move(reader));
    RendGraphVK graph(&rend_data);

    EXPECT_NE(build_error(graph, desc).find("resource 'dynamic_nodes' without a chain of edges on that resource"),
        std::string::npos);
}

TEST(WBERendGraphVKTest, TaskFactoriesReceiveTheContextOfTheRendData) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    rend_data.swap_chain_extent = {.width = 640, .height = 360};
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    VkExtent2D received_extent{};
    desc.add_task("task", [&alloc, &received_extent](const RendGraphContextVK& p_context) {
        received_extent = p_context.swap_chain_extent;
        return make_unique<RTGraphEdgeProbeVK>(
            &alloc, &alloc, QueueFamilyVK::GRAPHICS, std::vector<std::string>{}, std::vector<std::string>{std::string(TEST_LOGICAL_RESOURCE)});
    });
    RendGraphVK graph(&rend_data);

    EXPECT_NE(build_error(graph, desc).find(TEST_VALIDATION_PASSED_MESSAGE), std::string::npos);
    EXPECT_EQ(received_extent.width, 640U);
    EXPECT_EQ(received_extent.height, 360U);
    const RendGraphContextVK expected_context{.swap_chain_extent = {.width = 640, .height = 360}};
    EXPECT_TRUE(graph.get_context() == expected_context);
}

namespace {

// Bindings that record the renderer facts they receive and supply one resource, one native task, and a context entry.
class RecordingBindingsVK final : public IRendGraphBindingsVK {
public:
    RecordingBindingsVK(RendAlloc* p_alloc, Ref<RendObjectContainerVK> p_container)
        : IRendGraphBindingsVK("recording"), alloc(p_alloc), container(std::move(p_container)) {
    }

    virtual RendGraphPreparedBindingsVK prepare(const RendContextVK& p_rend_context, const RendRecipeContextVK& p_recipe_context) override {
        received_rend_context = &p_rend_context;
        received_facts = p_recipe_context;
        ++prepare_count;
        RendGraphPreparedBindingsVK result;
        result.context = DataValue::make_map();
        result.context.set_value("label", std::string("bound"));
        // The graph replaces the renderer facts with its own.
        result.context.set_value("frames_in_flight", int64_t{99});
        result.imports.add(TEST_LOGICAL_RESOURCE, *container);
        result.imports.add_task(RendTaskDescVK(alloc, "native", [this](const RendGraphContextVK& /*p_context*/) {
            return make_unique<RTGraphEdgeProbeVK>(alloc,
                alloc,
                QueueFamilyVK::GRAPHICS,
                std::vector<std::string>{},
                std::vector<std::string>{std::string(TEST_LOGICAL_RESOURCE)});
        }));
        return result;
    }

    const RendContextVK* received_rend_context = nullptr;
    RendRecipeContextVK received_facts;
    uint32_t prepare_count = 0;

private:
    RendAlloc* alloc;
    Ref<RendObjectContainerVK> container;
};

// Imports the bindings' resource and task, and fails unless the context and parameters reached build(context, parameters).
constexpr std::string_view TEST_RECIPE_SOURCE = R"(
local exports = {}
function exports.build(context, parameters)
    if context.label ~= "bound" then error("context.label is " .. tostring(context.label)) end
    if context.frames_in_flight ~= 2 then error("context.frames_in_flight is " .. tostring(context.frames_in_flight)) end
    if context.swap_chain_extent.width ~= 800 then error("context.swap_chain_extent.width is wrong") end
    if parameters.task ~= "native" then error("parameters.task is " .. tostring(parameters.task)) end
    return { imports = { "logical" }, tasks = { { id = parameters.task, import = true } } }
end
return exports
)";

class WBERendGraphVKRecipeTest : public ::testing::Test {
protected:
    virtual void SetUp() override {
        global = make_test_global();
    }

    virtual void TearDown() override {
        global.reset();
    }

    std::unique_ptr<Global> global;
};

} // namespace

TEST_F(WBERendGraphVKRecipeTest, RecipeSourcesPrepareTheBindingsAndRunTheRecipeWithTheParameters) {
    RendAlloc alloc(WBE_KI_B(256));
    RendContextVK rend_data(&alloc);
    rend_data.swap_chain_extent = {.width = 800, .height = 600};
    rend_data.frames_in_flight = 2;
    StackAlloc storage(WBE_KI_B(4));
    RefRes<char> source(storage.allocate(TEST_RECIPE_SOURCE.size() + 1), &storage);
    std::memcpy(source.get(), TEST_RECIPE_SOURCE.data(), TEST_RECIPE_SOURCE.size());
    source.get()[TEST_RECIPE_SOURCE.size()] = '\0';
    RefRes<RUScript> script(storage.allocate(sizeof(RUScript)), &storage);
    new (script.get()) RUScript{};
    script->resource_key = "tests.recording_recipe";
    script->source = source;
    script->size = TEST_RECIPE_SOURCE.size() + 1;
    auto bindings = make_ref<RecordingBindingsVK>(&alloc, &alloc, make_test_container(&alloc, &rend_data));
    DataValue parameters = DataValue::make_map();
    parameters.set_value("task", std::string("native"));
    RendGraphVK graph(&rend_data, "recording_graph");

    std::string error;
    try {
        graph.build_from_recipe(RendGraphRecipeSourceVK{.recipe = script, .parameters = parameters, .bindings = bindings});
    } catch (const std::runtime_error& p_error) {
        error = p_error.what();
    }
    // The recipe's descriptor validated; the build stops at command pool creation without a device.
    EXPECT_NE(error.find(TEST_VALIDATION_PASSED_MESSAGE), std::string::npos) << error;
    EXPECT_EQ(bindings->prepare_count, 1U);
    EXPECT_EQ(bindings->received_rend_context, &rend_data);
    EXPECT_EQ(bindings->received_facts.frames_in_flight, 2U);
    EXPECT_EQ(bindings->received_facts.swap_chain_extent.width, 800U);
    EXPECT_EQ(bindings->received_facts.swap_chain_extent.height, 600U);
    EXPECT_TRUE(graph.get_bindings() == bindings);
    EXPECT_EQ(graph.get_type_uuid(), TypeUUID<RendGraphVK>::UUID);
}

TEST_F(WBERendGraphVKRecipeTest, RecipeSourcesRequireARecipeBindingsAndMapParameters) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    StackAlloc storage(WBE_KI_B(1));
    RefRes<RUScript> script(storage.allocate(sizeof(RUScript)), &storage);
    new (script.get()) RUScript{};
    auto bindings = make_ref<RecordingBindingsVK>(&alloc, &alloc, make_test_container(&alloc, &rend_data));
    RendGraphVK graph(&rend_data);
    EXPECT_THROW(graph.build_from_recipe(RendGraphRecipeSourceVK{.bindings = bindings}), std::runtime_error);
    EXPECT_THROW(graph.build_from_recipe(RendGraphRecipeSourceVK{.recipe = script}), std::runtime_error);
    EXPECT_THROW(graph.build_from_recipe(
                     RendGraphRecipeSourceVK{.recipe = script, .parameters = DataValue::make_list(), .bindings = bindings}),
        std::runtime_error);
    EXPECT_EQ(bindings->prepare_count, 0U);
    EXPECT_TRUE(graph.get_bindings() == MEM_NULL);
}

TEST(WBERendGraphVKTest, InstallsTheDefaultResourceRegistrators) {
    RendAlloc alloc(WBE_KI_B(16));
    RendContextVK rend_data(&alloc);
    RendGraphVK graph(&rend_data);
    for (TypeID description_type : {TypeUUID<RDParamsMeshVK>::UUID,
             TypeUUID<RDParamsImageVK>::UUID,
             TypeUUID<RDParamsGraphicsPipelineVK>::UUID,
             TypeUUID<RDParamsComputePipelineVK>::UUID}) {
        EXPECT_TRUE(graph.has_resource_registrator(description_type)) << description_type;
    }
    EXPECT_FALSE(graph.contains_registered_resource("mesh"));
    // Without a renderer, the default registrators cannot compile.
    EXPECT_THROW((void)graph.register_mesh("mesh", RDParamsMeshVK{.mesh = "missing"}), std::runtime_error);
    EXPECT_FALSE(graph.contains_registered_resource("mesh"));
}

TEST(WBERendGraphVKTest, HandleRegistrationsRequireLoadedResources) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    RendGraphVK graph(&rend_data);
    const auto error = [](const auto& p_register) {
        try {
            p_register();
        } catch (const std::runtime_error& p_error) {
            return std::string(p_error.what());
        }
        return std::string();
    };
    EXPECT_NE(error([&graph] {
        (void)graph.register_mesh("mesh", RefRes<RUMesh3D>());
    }).find("the mesh handle is null"),
        std::string::npos);
    EXPECT_NE(error([&graph] {
        (void)graph.register_compute_pipeline("compute", RefRes<RUShader>());
    }).find("the shader handle is null"),
        std::string::npos);
    // An anonymous resource has no key a description could name.
    const MemID pipeline_id = create_obj<RUGraphicsPipeline>(alloc);
    EXPECT_NE(error([&graph] {
        (void)graph.register_image("texture", RefRes<RUTexture>());
    }).find("the texture handle is null"),
        std::string::npos);
    EXPECT_NE(error([&graph, &alloc, pipeline_id] {
        (void)graph.register_graphics_pipeline(
            "pipeline", RefRes<RUGraphicsPipeline>(pipeline_id, &alloc), RDParamsGraphicsPipelineVK{});
    }).find("has no resource key"),
        std::string::npos);
    destroy_obj<RUGraphicsPipeline>(alloc, pipeline_id);
    EXPECT_FALSE(graph.contains_registered_resource("texture"));
    EXPECT_FALSE(graph.contains_registered_resource("pipeline"));
}

TEST(WBERendGraphVKTest, RejectsRebuildsOfGraphsThatWereNotBuilt) {
    RendAlloc alloc(WBE_KI_B(16));
    RendContextVK rend_data(&alloc);
    RendGraphVK graph(&rend_data);
    EXPECT_THROW(graph.rebuild(), std::runtime_error);
}

TEST(WBERendGraphVKTest, IsARendObjectWithItsConcreteTypeUUID) {
    RendAlloc alloc(WBE_KI_B(16));
    RendContextVK rend_data(&alloc);
    Ref<RendGraphVK> graph = make_ref<RendGraphVK>(&alloc, &rend_data);
    Ref<IRendObject> object = graph;
    EXPECT_EQ(object->get_type_uuid(), TypeUUID<RendGraphVK>::UUID);
    EXPECT_TRUE(object.dynamic_cast_ref<IRendGraph>() == graph);
    EXPECT_EQ(graph->get_rend_context(), &rend_data);
    EXPECT_TRUE(graph->get_bindings() == MEM_NULL);
}

TEST(WBERendGraphVKTest, RejectsTaskFactoriesThatReturnNull) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_task("task", [](const RendGraphContextVK& /*p_context*/) {
        return Unique<IRendTaskVK>();
    });
    RendGraphVK graph(&rend_data);

    EXPECT_NE(build_error(graph, desc).find("task factory returned null for: task"), std::string::npos);
}

TEST(WBERendGraphVKTest, RejectsAliasesThatTheTaskNeverUses) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    RendTaskDescVK task = make_probe_desc(&alloc, "task", QueueFamilyVK::GRAPHICS, {"input"}, {});
    task.alias("input", TEST_LOGICAL_RESOURCE).alias("unused", TEST_LOGICAL_RESOURCE);
    desc.add_task(std::move(task));
    RendGraphVK graph(&rend_data);

    EXPECT_NE(build_error(graph, desc).find("task 'task' maps resource 'unused', which the task never uses"), std::string::npos);
}

TEST(WBERendGraphVKTest, ResourceEdgeChainOrdersConflictingAccesses) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    // Declared out of execution order to exercise topological sorting.
    desc.add_task(make_probe_desc(&alloc, "reader", QueueFamilyVK::GRAPHICS, {TEST_LOGICAL_RESOURCE}, {}));
    desc.add_task(make_probe_desc(&alloc, "writer", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    desc.add_task(make_probe_desc(&alloc, "rewriter", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    desc.get_task("writer").connect("reader", TEST_LOGICAL_RESOURCE);
    desc.get_task("reader").connect("rewriter", TEST_LOGICAL_RESOURCE);
    RendGraphVK graph(&rend_data);

    EXPECT_NE(build_error(graph, desc).find(TEST_VALIDATION_PASSED_MESSAGE), std::string::npos);
}

TEST(WBERendGraphVKTest, DumpGraphMetadataIsEmptyBeforeBuild) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphVK graph(&rend_data);

    RendGraphMetadataVK metadata = graph.dump_graph_metadata();
    EXPECT_TRUE(metadata.tasks.empty());
    EXPECT_TRUE(metadata.edges.empty());
}

TEST(WBERendGraphVKTest, DumpGraphMetadataListsTasksInExecutionOrderAndEdgeAccesses) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    // Declared out of execution order to check that the metadata follows the topological order.
    desc.add_task(make_probe_desc(&alloc, "reader", QueueFamilyVK::GRAPHICS, {TEST_LOGICAL_RESOURCE}, {}));
    desc.add_task(make_probe_desc(&alloc, "writer", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    desc.add_task(make_probe_desc(&alloc, "rewriter", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    desc.get_task("writer").connect("reader", TEST_LOGICAL_RESOURCE);
    desc.get_task("reader").connect("rewriter", TEST_LOGICAL_RESOURCE);
    RendGraphVK graph(&rend_data);
    // The edges are resolved before command-buffer allocation, which fails without a device.
    ASSERT_NE(build_error(graph, desc).find(TEST_VALIDATION_PASSED_MESSAGE), std::string::npos);

    RendGraphMetadataVK metadata = graph.dump_graph_metadata();
    ASSERT_EQ(metadata.tasks.size(), 3);
    EXPECT_EQ(metadata.tasks[0].task_id, "writer");
    EXPECT_EQ(metadata.tasks[1].task_id, "reader");
    EXPECT_EQ(metadata.tasks[2].task_id, "rewriter");
    for (uint64_t i = 0; i < metadata.tasks.size(); ++i) {
        EXPECT_EQ(metadata.tasks[i].topological_order, i);
        EXPECT_EQ(metadata.tasks[i].queue_family, QueueFamilyVK::GRAPHICS);
    }

    ASSERT_EQ(metadata.edges.size(), 2);
    auto find_edge = [&metadata](std::string_view p_from, std::string_view p_to) {
        auto edge = std::ranges::find_if(metadata.edges, [p_from, p_to](const RendGraphEdgeMetadataVK& p_edge) {
            return p_edge.from_task_id == p_from && p_edge.to_task_id == p_to;
        });
        return edge == metadata.edges.end() ? nullptr : &*edge;
    };
    const RendGraphEdgeMetadataVK* write_read = find_edge("writer", "reader");
    ASSERT_NE(write_read, nullptr);
    EXPECT_EQ(write_read->resource_id, TEST_LOGICAL_RESOURCE);
    EXPECT_TRUE(write_read->from_task_writes);
    EXPECT_FALSE(write_read->to_task_writes);
    EXPECT_FALSE(write_read->cross_queue);
    const RendGraphEdgeMetadataVK* read_write = find_edge("reader", "rewriter");
    ASSERT_NE(read_write, nullptr);
    EXPECT_EQ(read_write->resource_id, TEST_LOGICAL_RESOURCE);
    EXPECT_FALSE(read_write->from_task_writes);
    EXPECT_TRUE(read_write->to_task_writes);
    EXPECT_FALSE(read_write->cross_queue);
}

TEST(WBERendGraphVKTest, EdgesOnAnotherResourceDoNotOrderAConflict) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_resource(TEST_SHARED_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_task(
        make_probe_desc(&alloc, "writer", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE, TEST_SHARED_RESOURCE}));
    desc.add_task(
        make_probe_desc(&alloc, "reader", QueueFamilyVK::GRAPHICS, {TEST_LOGICAL_RESOURCE, TEST_SHARED_RESOURCE}, {}));
    desc.get_task("writer").connect("reader", TEST_SHARED_RESOURCE);
    RendGraphVK graph(&rend_data);

    EXPECT_NE(build_error(graph, desc).find("resource 'logical' without a chain of edges on that resource"), std::string::npos);
}

TEST(WBERendGraphVKTest, RejectsCyclicResourceEdges) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_task(make_probe_desc(&alloc, "first", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    desc.add_task(make_probe_desc(&alloc, "second", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    desc.get_task("first").connect("second", TEST_LOGICAL_RESOURCE);
    desc.get_task("second").connect("first", TEST_LOGICAL_RESOURCE);
    RendGraphVK graph(&rend_data);

    const std::string message = build_error(graph, desc);
    EXPECT_NE(message.find("contains a cycle"), std::string::npos);
    EXPECT_NE(message.find("'first'"), std::string::npos);
    EXPECT_NE(message.find("'second'"), std::string::npos);
}

TEST(WBERendGraphVKTest, RejectsSelfEdgesUnknownTasksAndUndeclaredResources) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    auto make_desc = [&] {
        RendGraphDescVK desc(&alloc);
        desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
        desc.add_task(make_probe_desc(&alloc, "task", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
        desc.add_task(make_probe_desc(&alloc, "other", QueueFamilyVK::GRAPHICS, {}, {}));
        return desc;
    };
    RendGraphVK graph(&rend_data);

    RendGraphDescVK self_edge = make_desc();
    self_edge.get_task("task").connect("task", TEST_LOGICAL_RESOURCE);
    EXPECT_NE(build_error(graph, self_edge).find("connects to itself"), std::string::npos);

    RendGraphDescVK unknown_task = make_desc();
    unknown_task.get_task("task").connect("missing", TEST_LOGICAL_RESOURCE);
    EXPECT_NE(build_error(graph, unknown_task).find("connects to unknown task 'missing'"), std::string::npos);

    RendGraphDescVK undeclared_resource = make_desc();
    undeclared_resource.get_task("task").connect("other", TEST_MISSING_RESOURCE);
    EXPECT_NE(build_error(graph, undeclared_resource).find("uses undeclared resource 'missing'"), std::string::npos);
}

TEST(WBERendGraphVKTest, RejectsEdgesWhoseTasksDoNotAccessTheResource) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    auto make_desc = [&] {
        RendGraphDescVK desc(&alloc);
        desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
        desc.add_task(make_probe_desc(&alloc, "accessor", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
        desc.add_task(make_probe_desc(&alloc, "bystander", QueueFamilyVK::GRAPHICS, {}, {}));
        return desc;
    };
    RendGraphVK graph(&rend_data);

    RendGraphDescVK unused_by_source = make_desc();
    unused_by_source.get_task("bystander").connect("accessor", TEST_LOGICAL_RESOURCE);
    EXPECT_NE(build_error(graph, unused_by_source).find("but task 'bystander' does not access it"), std::string::npos);

    RendGraphDescVK unused_by_destination = make_desc();
    unused_by_destination.get_task("accessor").connect("bystander", TEST_LOGICAL_RESOURCE);
    EXPECT_NE(build_error(graph, unused_by_destination).find("but task 'bystander' does not access it"), std::string::npos);
}

TEST(WBERendGraphVKTest, GraphOwnedResourcesRejectDuplicateIdsBeforeAllocation) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphVK graph(&rend_data);

    RendGraphDescVK imported_duplicate(&alloc);
    imported_duplicate.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    imported_duplicate.add_buffer(TEST_LOGICAL_RESOURCE, [=](const RendGraphContextVK& /*p_context*/) {
        return RendBufferDescVK{.usage = RendResPrimaryUsageVK::BUFFER_STORAGE, .size = 16};
    });
    EXPECT_NE(build_error(graph, imported_duplicate).find("duplicate resource id: logical"), std::string::npos);

    RendGraphDescVK owned_duplicate(&alloc);
    owned_duplicate.add_image(TEST_SHARED_RESOURCE, [=](const RendGraphContextVK& /*p_context*/) {
        return RendImageDescVK{.format = VK_FORMAT_R8G8B8A8_UNORM, .extent = {.width = 4, .height = 4, .depth = 1}};
    });
    owned_duplicate.add_sampler(TEST_SHARED_RESOURCE);
    EXPECT_NE(build_error(graph, owned_duplicate).find("duplicate resource id: shared"), std::string::npos);
}

TEST(WBERendGraphVKTest, RejectsInvalidGraphOwnedResourceDeclarations) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphVK graph(&rend_data);

    RendGraphDescVK image_usage_buffer(&alloc);
    image_usage_buffer.add_buffer("buffer", [=](const RendGraphContextVK& /*p_context*/) {
        return RendBufferDescVK{.usage = RendResPrimaryUsageVK::IMAGE_READ_WRITE, .size = 16};
    });
    EXPECT_NE(build_error(graph, image_usage_buffer).find("buffer 'buffer' has a non-buffer primary usage"), std::string::npos);

    RendGraphDescVK empty_buffer(&alloc);
    empty_buffer.add_buffer("buffer", [=](const RendGraphContextVK& /*p_context*/) {
        return RendBufferDescVK{.usage = RendResPrimaryUsageVK::BUFFER_UNIFORM};
    });
    EXPECT_NE(build_error(graph, empty_buffer).find("buffer 'buffer' has no size"), std::string::npos);

    RendGraphDescVK oversized_data(&alloc);
    oversized_data.add_buffer("buffer", [=](const RendGraphContextVK& /*p_context*/) {
        return RendBufferDescVK{.usage = RendResPrimaryUsageVK::BUFFER_UNIFORM, .size = 2, .init_data = {1, 2, 3}};
    });
    EXPECT_NE(build_error(graph, oversized_data).find("initial data of buffer 'buffer' exceeds its size"), std::string::npos);

    RendGraphDescVK buffer_usage_image(&alloc);
    buffer_usage_image.add_image("image", [=](const RendGraphContextVK& /*p_context*/) {
        return RendImageDescVK{.usage = RendResPrimaryUsageVK::BUFFER_STORAGE,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {.width = 4, .height = 4, .depth = 1}};
    });
    EXPECT_NE(build_error(graph, buffer_usage_image).find("image 'image' has a non-image primary usage"), std::string::npos);

    RendGraphDescVK undefined_image(&alloc);
    undefined_image.add_image("image", [=](const RendGraphContextVK& /*p_context*/) {
        return RendImageDescVK{.extent = {.width = 4, .height = 4, .depth = 1}};
    });
    EXPECT_NE(build_error(graph, undefined_image).find("image 'image' has an undefined format"), std::string::npos);
}

TEST(WBERendGraphVKTest, ImageViewsMustViewDeclaredImages) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphVK graph(&rend_data);

    RendGraphDescVK missing_image(&alloc);
    missing_image.add_image_view("view", "missing_image");
    EXPECT_NE(build_error(graph, missing_image).find("image view 'view' views 'missing_image', which is not a declared image resource"),
        std::string::npos);

    RendGraphDescVK non_image(&alloc);
    non_image.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    non_image.add_image_view("view", TEST_LOGICAL_RESOURCE);
    EXPECT_NE(build_error(graph, non_image).find("which is not a declared image resource"), std::string::npos);
}

namespace {

// A subgraph whose `first` writes its `data` port and `second` then rewrites it.
RendSubgraphDescVK make_rewriting_subgraph(RendAlloc* p_alloc) {
    RendSubgraphDescVK subgraph(p_alloc);
    subgraph.add_port("data");
    subgraph.graph.add_task(make_probe_desc(p_alloc, "first", QueueFamilyVK::GRAPHICS, {}, {"data"}));
    subgraph.graph.add_task(make_probe_desc(p_alloc, "second", QueueFamilyVK::GRAPHICS, {"data"}, {"data"}));
    subgraph.graph.get_task("first").connect("second", "data");
    return subgraph;
}

const std::array<RendSubgraphPortBindingVK, 1> LOGICAL_DATA_PORT{{{.port = "data", .resource = TEST_LOGICAL_RESOURCE}}};

bool has_edge(const RendGraphMetadataVK& p_metadata, std::string_view p_from, std::string_view p_to, std::string_view p_resource_id) {
    return std::ranges::any_of(p_metadata.edges, [&](const RendGraphEdgeMetadataVK& p_edge) {
        return p_edge.from_task_id == p_from && p_edge.to_task_id == p_to && p_edge.resource_id == p_resource_id;
    });
}

} // namespace

TEST(WBERendGraphVKTest, SubgraphEdgesConnectToTheEntriesAndLeaveFromTheExits) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_task(make_probe_desc(&alloc, "producer", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    desc.add_task(make_probe_desc(&alloc, "consumer", QueueFamilyVK::GRAPHICS, {TEST_LOGICAL_RESOURCE}, {}));
    desc.add_subgraph("sort", make_rewriting_subgraph(&alloc), LOGICAL_DATA_PORT).connect("consumer", TEST_LOGICAL_RESOURCE);
    desc.get_task("producer").connect("sort", TEST_LOGICAL_RESOURCE);
    RendGraphVK graph(&rend_data);
    // The chain through the subgraph orders every conflicting access.
    ASSERT_NE(build_error(graph, desc).find(TEST_VALIDATION_PASSED_MESSAGE), std::string::npos);

    const RendGraphMetadataVK metadata = graph.dump_graph_metadata();
    ASSERT_EQ(metadata.edges.size(), 3U);
    EXPECT_TRUE(has_edge(metadata, "producer", "sort.first", TEST_LOGICAL_RESOURCE));
    EXPECT_TRUE(has_edge(metadata, "sort.first", "sort.second", TEST_LOGICAL_RESOURCE));
    EXPECT_TRUE(has_edge(metadata, "sort.second", "consumer", TEST_LOGICAL_RESOURCE));
}

TEST(WBERendGraphVKTest, UnconnectedSubgraphAccessesStillConflict) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_task(make_probe_desc(&alloc, "producer", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    desc.add_subgraph("sort", make_rewriting_subgraph(&alloc), LOGICAL_DATA_PORT);
    RendGraphVK graph(&rend_data);

    EXPECT_NE(build_error(graph, desc).find("without a chain of edges on that resource"), std::string::npos);
}

TEST(WBERendGraphVKTest, SubgraphsConnectToSubgraphsAndExposeTheirMembers) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_task(make_probe_desc(&alloc, "reader", QueueFamilyVK::GRAPHICS, {TEST_LOGICAL_RESOURCE}, {}));
    desc.add_subgraph("upstream", make_rewriting_subgraph(&alloc), LOGICAL_DATA_PORT).connect("downstream", TEST_LOGICAL_RESOURCE);
    desc.add_subgraph("downstream", make_rewriting_subgraph(&alloc), LOGICAL_DATA_PORT);
    // A member is an ordinary task, which other tasks may connect to directly.
    desc.get_task("downstream.second").connect("reader", TEST_LOGICAL_RESOURCE);
    RendGraphVK graph(&rend_data);
    ASSERT_NE(build_error(graph, desc).find(TEST_VALIDATION_PASSED_MESSAGE), std::string::npos);

    const RendGraphMetadataVK metadata = graph.dump_graph_metadata();
    EXPECT_TRUE(has_edge(metadata, "upstream.second", "downstream.first", TEST_LOGICAL_RESOURCE));
    EXPECT_TRUE(has_edge(metadata, "downstream.second", "reader", TEST_LOGICAL_RESOURCE));
}

TEST(WBERendGraphVKTest, NestedSubgraphsResolveToTheirInnermostTasks) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    // `post` runs the nested `sort` on its port, then `finish` reads the result.
    RendSubgraphDescVK post(&alloc);
    post.add_port("target");
    post.graph.add_task(make_probe_desc(&alloc, "finish", QueueFamilyVK::GRAPHICS, {"target"}, {}));
    const std::array<RendSubgraphPortBindingVK, 1> sort_ports{{{.port = "data", .resource = "target"}}};
    post.graph.add_subgraph("sort", make_rewriting_subgraph(&alloc), sort_ports).connect("finish", "target");
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_task(make_probe_desc(&alloc, "producer", QueueFamilyVK::GRAPHICS, {}, {TEST_LOGICAL_RESOURCE}));
    const std::array<RendSubgraphPortBindingVK, 1> post_ports{{{.port = "target", .resource = TEST_LOGICAL_RESOURCE}}};
    desc.add_subgraph("post", post, post_ports);
    desc.get_task("producer").connect("post", TEST_LOGICAL_RESOURCE);
    RendGraphVK graph(&rend_data);
    ASSERT_NE(build_error(graph, desc).find(TEST_VALIDATION_PASSED_MESSAGE), std::string::npos);

    const RendGraphMetadataVK metadata = graph.dump_graph_metadata();
    ASSERT_EQ(metadata.edges.size(), 3U);
    EXPECT_TRUE(has_edge(metadata, "producer", "post.sort.first", TEST_LOGICAL_RESOURCE));
    EXPECT_TRUE(has_edge(metadata, "post.sort.first", "post.sort.second", TEST_LOGICAL_RESOURCE));
    EXPECT_TRUE(has_edge(metadata, "post.sort.second", "post.finish", TEST_LOGICAL_RESOURCE));
    // Each task names its innermost subgraph, and each subgraph its parent, parents first.
    const auto subgraph_of = [&metadata](std::string_view p_task_id) {
        return std::ranges::find_if(metadata.tasks, [p_task_id](const RendGraphTaskMetadataVK& p_task) {
            return p_task.task_id == p_task_id;
        })->subgraph_id;
    };
    EXPECT_EQ(subgraph_of("producer"), "");
    EXPECT_EQ(subgraph_of("post.finish"), "post");
    EXPECT_EQ(subgraph_of("post.sort.first"), "post.sort");
    ASSERT_EQ(metadata.subgraphs.size(), 2U);
    EXPECT_EQ(metadata.subgraphs[0].subgraph_id, "post");
    EXPECT_EQ(metadata.subgraphs[0].parent_subgraph_id, "");
    EXPECT_EQ(metadata.subgraphs[1].subgraph_id, "post.sort");
    EXPECT_EQ(metadata.subgraphs[1].parent_subgraph_id, "post");
}

TEST(WBERendGraphVKTest, RejectsSubgraphEdgesOnResourcesNoMemberAccesses) {
    RendAlloc alloc(WBE_KI_B(128));
    RendContextVK rend_data(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(TEST_LOGICAL_RESOURCE, *make_test_container(&alloc, &rend_data));
    desc.add_resource("other", *make_test_container(&alloc, &rend_data));
    desc.add_task(make_probe_desc(&alloc, "consumer", QueueFamilyVK::GRAPHICS, {"other"}, {}));
    desc.add_subgraph("sort", make_rewriting_subgraph(&alloc), LOGICAL_DATA_PORT).connect("consumer", "other");
    RendGraphVK graph(&rend_data);

    EXPECT_NE(build_error(graph, desc).find("subgraph 'sort' is connected on resource 'other', but none of its tasks accesses it"),
        std::string::npos);
}

} // namespace WhiteBirdEngine
