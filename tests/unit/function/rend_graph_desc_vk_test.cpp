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
#include "core/alloc/i_alloc.hh"
#include "core/core_utils.hh"
#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/function_utils.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_bindable_rend_resource_vk.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/mesh_resources_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/rend_vk/rend_resource_list_vk.hh"
#include "function/rend_vk/ro_buffer_vk.hh"
#include "function/render/roc_single.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/** @brief Minimal render task used to verify that task factories create and destroy tasks. */
class RTDirectDescVK final : public IRendTaskVK {
public:
    explicit RTDirectDescVK(bool* p_destroyed) : destroyed(p_destroyed) {
    }
    virtual ~RTDirectDescVK() override {
        *destroyed = true;
    }
    RTDirectDescVK(const RTDirectDescVK&) = delete;
    RTDirectDescVK(RTDirectDescVK&&) noexcept = delete;
    RTDirectDescVK& operator=(const RTDirectDescVK&) = delete;
    RTDirectDescVK& operator=(RTDirectDescVK&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void append_accesses(Vector<ResAccessVK, RendAlloc>& /*p_reads*/,
        Vector<ResAccessVK, RendAlloc>& /*p_writes*/) const override {
    }

    virtual void record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) override {
    }

private:
    bool* destroyed;
};

/** @brief Minimal render object that fills the container used to test container binding. */
class TestContainerResourceVK final : public IRendObject {
public:
    TestContainerResourceVK() = default;
    virtual ~TestContainerResourceVK() override = default;
    TestContainerResourceVK(const TestContainerResourceVK&) = delete;
    TestContainerResourceVK(TestContainerResourceVK&&) noexcept = delete;
    TestContainerResourceVK& operator=(const TestContainerResourceVK&) = delete;
    TestContainerResourceVK& operator=(TestContainerResourceVK&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return TypeUUID<ROBufferVK>::UUID;
    }
};

TEST(WBERendGraphDescVKTest, DescStoresTaskFactory) {
    RendAlloc alloc(WBE_KI_B(64));
    bool destroyed = false;
    int created_count = 0;
    RendTaskDescVK desc(&alloc, "direct_task", [&alloc, &destroyed, &created_count](const RendGraphContextVK& /*p_context*/) {
        ++created_count;
        return make_unique<RTDirectDescVK>(&alloc, &destroyed);
    });
    const RendTaskDescVK& copied_desc = desc;

    EXPECT_EQ(copied_desc.task_id, "direct_task");
    ASSERT_NE(copied_desc.factory, nullptr);
    EXPECT_EQ(created_count, 0);

    Unique<IRendTaskVK> task = copied_desc.factory(RendGraphContextVK{});

    EXPECT_FALSE(task.is_null());
    EXPECT_EQ(created_count, 1);
    EXPECT_FALSE(destroyed);
    task.reset();
    EXPECT_TRUE(destroyed);
}

TEST(WBERendGraphDescVKTest, TaskFactoryConstructorRejectsNullFactory) {
    RendAlloc alloc(WBE_KI_B(64));

    EXPECT_THROW(RendTaskDescVK(&alloc, "null_task", nullptr), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, FluentAliasesAndConnectionsAreStored) {
    RendAlloc alloc(WBE_KI_B(64));
    RendTaskDescVK desc(&alloc);
    desc.task_id = "run_lighting";
    desc.alias("rend_target_colors[0]", "lighting_output").connect("run_bloom", "lighting_output");

    ASSERT_EQ(desc.next.size(), 1U);
    EXPECT_EQ(desc.next.front().next_task_id, "run_bloom");
    EXPECT_EQ(desc.next.front().res_id, "lighting_output");
    EXPECT_EQ(desc.resource_map.resolve("rend_target_colors[0]"), "lighting_output");
    EXPECT_EQ(desc.resource_map.resolve("gbuffer_albedo"), "gbuffer_albedo");
    EXPECT_THROW(desc.alias("rend_target_colors[0]", "other"), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, GetTaskReturnsStoredDescForLaterConnections) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK desc(&alloc);
    RendTaskDescVK task(&alloc);
    task.task_id = "producer";
    desc.add_task(std::move(task));

    desc.get_task("producer").connect("consumer", "buffer");

    ASSERT_EQ(desc.tasks.front().next.size(), 1U);
    EXPECT_EQ(desc.tasks.front().next.front().next_task_id, "consumer");
    EXPECT_EQ(desc.tasks.front().next.front().res_id, "buffer");
    EXPECT_THROW(desc.get_task("missing"), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, ResourceListIsStoredAsAnAggregateBinding) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK desc(&alloc);
    auto resource_list = make_ref<RendResourceListVK>(&alloc, &alloc);

    desc.add_resource("textures", *resource_list);

    ASSERT_EQ(desc.resources.size(), 1U);
    EXPECT_EQ(desc.resources.front().res_id, "textures");
    EXPECT_EQ(desc.resources.front().resource_list, resource_list);
    EXPECT_EQ(desc.resources.front().container, MEM_NULL);
}

TEST(WBERendGraphDescVKTest, GraphOwnedResourceDeclarationsAreStoredForCompilation) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK desc(&alloc);

    desc
        .add_buffer("camera",
            [=](const RendGraphContextVK& /*p_context*/) {
                return RendBufferDescVK{.usage = RendResPrimaryUsageVK::BUFFER_UNIFORM,
                    .domain = DeviceDomain::HOST_DEVICE,
                    .per_frame = true,
                    .init_data = {1, 2}};
            })
        .add_image("albedo_image",
            [=](const RendGraphContextVK& /*p_context*/) {
                return RendImageDescVK{.usage = RendResPrimaryUsageVK::IMAGE_REND_TARGET,
                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                    .extent = {.width = 8, .height = 8, .depth = 1},
                    .mip_levels = 2};
            })
        .add_image_view("albedo",
            "albedo_image",
            [=](const RendGraphContextVK& /*p_context*/) {
                return RendImageViewDescVK{.mip_level_count = VK_REMAINING_MIP_LEVELS};
            })
        .add_sampler("sampler", [=](const RendGraphContextVK& /*p_context*/) {
            return RendSamplerDescVK{.min_filter = VK_FILTER_NEAREST, .max_anisotropy = 4.0F};
        });

    ASSERT_EQ(desc.buffers.size(), 1U);
    EXPECT_EQ(desc.buffers.front().res_id, "camera");
    EXPECT_TRUE(desc.buffers.front().factory(RendGraphContextVK{}).per_frame);
    EXPECT_EQ(desc.buffers.front().factory(RendGraphContextVK{}).init_data.size(), 2U);
    ASSERT_EQ(desc.images.size(), 1U);
    EXPECT_EQ(desc.images.front().factory(RendGraphContextVK{}).mip_levels, 2U);
    ASSERT_EQ(desc.image_views.size(), 1U);
    EXPECT_EQ(desc.image_views.front().image_id, "albedo_image");
    EXPECT_EQ(desc.image_views.front().factory(RendGraphContextVK{}).mip_level_count, VK_REMAINING_MIP_LEVELS);
    ASSERT_EQ(desc.samplers.size(), 1U);
    EXPECT_EQ(desc.samplers.front().factory(RendGraphContextVK{}).min_filter, VK_FILTER_NEAREST);
    EXPECT_EQ(desc.samplers.front().factory(RendGraphContextVK{}).mag_filter, VK_FILTER_LINEAR);
    EXPECT_EQ(desc.samplers.front().factory(RendGraphContextVK{}).max_anisotropy, 4.0F);
    EXPECT_FALSE(desc.samplers.front().factory(RendGraphContextVK{}).compare_op.has_value());
    EXPECT_TRUE(desc.resources.empty());
}

TEST(WBERendGraphDescVKTest, GraphOwnedResourceDeclarationsRejectEmptyIds) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK desc(&alloc);

    EXPECT_THROW(desc.add_buffer("",
                     [=](const RendGraphContextVK& /*p_context*/) {
                         return RendBufferDescVK{};
                     }),
        std::runtime_error);
    EXPECT_THROW(desc.add_image("",
                     [=](const RendGraphContextVK& /*p_context*/) {
                         return RendImageDescVK{};
                     }),
        std::runtime_error);
    EXPECT_THROW(desc.add_image_view("view", ""), std::runtime_error);
    EXPECT_THROW(desc.add_sampler(""), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, GraphOwnedResourceDeclarationsRejectNullFactories) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK desc(&alloc);

    EXPECT_THROW(desc.add_buffer("buffer", nullptr), std::runtime_error);
    EXPECT_THROW(desc.add_image("image", nullptr), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, GraphOwnedResourceFactoriesReceiveTheContext) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK desc(&alloc);

    desc.add_image("image", [](const RendGraphContextVK& p_context) {
        return RendImageDescVK{.format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {.width = p_context.swap_chain_extent.width, .height = p_context.swap_chain_extent.height, .depth = 1}};
    });
    desc.add_image_view("view", "image");
    desc.add_sampler("sampler");

    const RendImageDescVK image_desc =
        desc.images.front().factory(RendGraphContextVK{.swap_chain_extent = {.width = 640, .height = 360}});
    EXPECT_EQ(image_desc.extent.width, 640U);
    EXPECT_EQ(image_desc.extent.height, 360U);
    EXPECT_TRUE(desc.image_views.front().factory(RendGraphContextVK{}) == RendImageViewDescVK{});
    EXPECT_TRUE(desc.samplers.front().factory(RendGraphContextVK{}) == RendSamplerDescVK{});
}

TEST(WBERendGraphDescVKTest, ComposesMemberResourceIds) {
    EXPECT_EQ(IBindableRendResourceVK::make_member_resource_id("sponza", MeshResourcesVK::NODES), "sponza.nodes");
}

TEST(WBERendGraphDescVKTest, IncompleteMeshResourcesRefuseToBind) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK desc(&alloc);
    RendContextVK rend_data(&alloc);
    MeshResourcesVK static_mesh(&rend_data);

    EXPECT_THROW(desc.add_resource("static_mesh", static_mesh), std::runtime_error);
    EXPECT_TRUE(desc.resources.empty());
}

TEST(WBERendGraphDescVKTest, NullContainerRefusesToBind) {
    RendAlloc alloc(WBE_KI_B(64));
    RendGraphDescVK desc(&alloc);

    EXPECT_THROW(desc.add_resource("container", Ref<IRendObjectContainer>(MEM_NULL)), std::runtime_error);
    EXPECT_TRUE(desc.resources.empty());
}

namespace {

RendTaskDescVK make_empty_task(RendAlloc* p_alloc, std::string_view p_task_id) {
    return RendTaskDescVK(p_alloc, p_task_id, [p_alloc](const RendGraphContextVK& /*p_context*/) {
        static bool destroyed = false;
        return make_unique<RTDirectDescVK>(p_alloc, &destroyed);
    });
}

Ref<ROCSingle> make_import_container(RendAlloc* p_alloc) {
    return make_ref<ROCSingle>(p_alloc, [p_alloc](uint32_t /*p_slot*/) {
        return make_ref<TestContainerResourceVK>(p_alloc).dynamic_cast_ref<IRendObject>();
    });
}

// A subgraph with a port, an owned buffer, an image and its view, and two connected tasks using all three.
RendSubgraphDescVK make_sort_subgraph(RendAlloc* p_alloc) {
    RendSubgraphDescVK subgraph(p_alloc);
    subgraph.add_port("color");
    subgraph.graph.add_buffer("keys", [](const RendGraphContextVK& /*p_context*/) {
        return RendBufferDescVK{};
    });
    subgraph.graph.add_image("mask_image", [](const RendGraphContextVK& /*p_context*/) {
        return RendImageDescVK{};
    });
    subgraph.graph.add_image_view("mask", "mask_image");
    subgraph.graph.add_image_view("color_copy", "color");
    subgraph.graph.add_task(make_empty_task(p_alloc, "register"));
    subgraph.graph.add_task(make_empty_task(p_alloc, "write"));
    subgraph.graph.get_task("register").alias("input", "color").connect("write", "keys").connect("write", "color");
    return subgraph;
}

} // namespace

TEST(WBERendGraphDescVKTest, ResourceMapScopesApplyAfterTheTaskBindingsWithoutBeingValidated) {
    RendAlloc alloc(WBE_KI_B(64));
    RendTaskResourceMap map(&alloc);
    map.bind("rend_target_colors[0]", "color");
    const std::array<RendTaskResourceMap::ScopeBinding, 2> inner{
        {{.source = "color", .target = "result_image"}, {.source = "keys", .target = "sort.keys"}}};
    map.enclose(inner);
    // An enclosing scope sees the inner scope's results: the inner `sort.keys` becomes `deferred.sort.keys`.
    const std::array<RendTaskResourceMap::ScopeBinding, 2> outer{
        {{.source = "sort.keys", .target = "deferred.sort.keys"}, {.source = "result_image", .target = "deferred.result"}}};
    map.enclose(outer);

    EXPECT_EQ(map.resolve("rend_target_colors[0]"), "deferred.result");
    EXPECT_EQ(map.resolve("keys"), "deferred.sort.keys");
    EXPECT_EQ(map.resolve("result_image"), "deferred.result");
    EXPECT_EQ(map.resolve("swap_chain"), "swap_chain");
    // Only the task's own binding has to be used.
    EXPECT_NO_THROW(map.validate_all_used("task"));
    const std::array<RendTaskResourceMap::ScopeBinding, 2> repeated{{{.source = "a", .target = "b"}, {.source = "a", .target = "c"}}};
    EXPECT_THROW(map.enclose(repeated), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, ResourceMapMergeKeepsSharedBindingsAndRejectsConflicts) {
    RendAlloc alloc(WBE_KI_B(64));
    RendTaskResourceMap map(&alloc);
    map.bind("input", "buffer_a");
    RendTaskResourceMap other(&alloc);
    other.bind("input", "buffer_a").bind("output", "image_b");
    map.merge(other);
    EXPECT_EQ(map.resolve("input"), "buffer_a");
    EXPECT_EQ(map.resolve("output"), "image_b");
    // The shared binding is kept once, so resolving both entries leaves none unused.
    EXPECT_NO_THROW(map.validate_all_used("task"));

    RendTaskResourceMap conflicting(&alloc);
    conflicting.bind("input", "buffer_c");
    EXPECT_THROW(map.merge(conflicting), std::runtime_error);
    RendTaskResourceMap scoped(&alloc);
    const std::array<RendTaskResourceMap::ScopeBinding, 1> scope{{{.source = "x", .target = "y"}}};
    scoped.enclose(scope);
    EXPECT_THROW(map.merge(scoped), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, SubgraphsArePrefixedAndResolveTheirPortsAndResources) {
    RendAlloc alloc(WBE_KI_B(128));
    RendGraphDescVK desc(&alloc);
    const std::array<RendSubgraphPortBindingVK, 1> ports{{{.port = "color", .resource = "result_image"}}};
    RendSubgraphVK& subgraph = desc.add_subgraph("sort", make_sort_subgraph(&alloc), ports);
    subgraph.connect("present", "result_image");

    ASSERT_EQ(desc.tasks.size(), 2U);
    EXPECT_EQ(desc.tasks[0].task_id, "sort.register");
    EXPECT_EQ(desc.tasks[1].task_id, "sort.write");
    const RendTaskDescVK& registering = desc.tasks[0];
    ASSERT_EQ(registering.next.size(), 2U);
    EXPECT_EQ(registering.next[0].next_task_id, "sort.write");
    EXPECT_EQ(registering.next[0].res_id, "sort.keys");
    EXPECT_EQ(registering.next[1].res_id, "result_image");
    // Ports, then declared resources, then the enclosing graph's resources.
    EXPECT_EQ(registering.resource_map.resolve("input"), "result_image");
    EXPECT_EQ(registering.resource_map.resolve("color"), "result_image");
    EXPECT_EQ(registering.resource_map.resolve("keys"), "sort.keys");
    EXPECT_EQ(registering.resource_map.resolve("mask"), "sort.mask");
    EXPECT_EQ(registering.resource_map.resolve("swap_chain"), "swap_chain");
    ASSERT_EQ(desc.buffers.size(), 1U);
    EXPECT_EQ(desc.buffers[0].res_id, "sort.keys");
    ASSERT_EQ(desc.image_views.size(), 2U);
    EXPECT_EQ(desc.image_views[0].res_id, "sort.mask");
    EXPECT_EQ(desc.image_views[0].image_id, "sort.mask_image");
    EXPECT_EQ(desc.image_views[1].image_id, "result_image");
    ASSERT_EQ(desc.subgraphs.size(), 1U);
    const RendSubgraphVK& record = desc.get_subgraph("sort");
    ASSERT_EQ(record.members.size(), 2U);
    EXPECT_EQ(record.members[1], "sort.write");
    ASSERT_EQ(record.next.size(), 1U);
    EXPECT_EQ(record.next[0].next_task_id, "present");
    EXPECT_THROW(desc.get_subgraph("missing"), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, StrictResourceMapsRejectUnboundIdentifiersAndExemptIdentityBindings) {
    RendAlloc alloc(WBE_KI_B(16));
    RendTaskResourceMap map(&alloc);
    map.bind("input", "buffer-0000abcd").bind("buffer-0000beef", "buffer-0000beef");
    // A map that is not strict resolves what it leaves unmapped to itself.
    EXPECT_EQ(map.resolve("camera_info"), "camera_info");
    map.require_bindings("draw");
    EXPECT_EQ(map.resolve("input"), "buffer-0000abcd");
    try {
        (void)map.resolve("camera_info");
        ADD_FAILURE() << "An unbound identifier resolved.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string_view(error.what()).find("task 'draw' uses resource 'camera_info', which it does not bind"), std::string_view::npos)
            << error.what();
    }
    // The identity binding is never resolved, which is no unused mapping; another unused binding is.
    EXPECT_NO_THROW(map.validate_all_used("draw"));
    map.bind("unused", "buffer-0000abcd");
    EXPECT_THROW(map.validate_all_used("draw"), std::runtime_error);
    EXPECT_THROW(map.require_bindings(""), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, DebugNamesAndExportsAreStoredAndSubgraphsPrefixTheirNames) {
    RendAlloc alloc(WBE_KI_B(128));
    RendGraphDescVK desc(&alloc);
    desc.add_sampler("sampler-0000abcd").set_debug_name("sampler-0000abcd", "linear").add_export("linear_sampler", "sampler-0000abcd");
    EXPECT_EQ(desc.debug_names.find("sampler-0000abcd")->second, "linear");
    EXPECT_EQ(desc.exports.find("linear_sampler")->second, "sampler-0000abcd");
    EXPECT_THROW(desc.set_debug_name("sampler-0000abcd", "again"), std::runtime_error);
    EXPECT_THROW(desc.set_debug_name("", "name"), std::runtime_error);
    EXPECT_THROW(desc.set_debug_name("sampler-0000abcd", ""), std::runtime_error);
    EXPECT_THROW(desc.add_export("linear_sampler", "sampler-0000abcd"), std::runtime_error);
    EXPECT_THROW(desc.add_export("", "sampler-0000abcd"), std::runtime_error);
    EXPECT_THROW(desc.add_export("other", ""), std::runtime_error);

    RendSubgraphDescVK subgraph(&alloc);
    subgraph.graph.add_sampler("sampler-0000beef").set_debug_name("sampler-0000beef", "nearest");
    desc.add_subgraph("post", subgraph, std::span<const RendSubgraphPortBindingVK>{});
    EXPECT_EQ(desc.debug_names.find("post.sampler-0000beef")->second, "post.nearest");
    // Subgraphs cannot export.
    RendSubgraphDescVK exporting(&alloc);
    exporting.graph.add_sampler("sampler-0000cafe").add_export("exported", "sampler-0000cafe");
    EXPECT_THROW(desc.add_subgraph("exporting", exporting, std::span<const RendSubgraphPortBindingVK>{}), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, NestedSubgraphsArePrefixedAgain) {
    RendAlloc alloc(WBE_KI_B(128));
    RendSubgraphDescVK outer(&alloc);
    outer.add_port("target");
    outer.graph.add_task(make_empty_task(&alloc, "begin"));
    const std::array<RendSubgraphPortBindingVK, 1> inner_ports{{{.port = "color", .resource = "target"}}};
    outer.graph.add_subgraph("sort", make_sort_subgraph(&alloc), inner_ports).connect("begin", "target");
    RendGraphDescVK desc(&alloc);
    const std::array<RendSubgraphPortBindingVK, 1> outer_ports{{{.port = "target", .resource = "result_image"}}};
    desc.add_subgraph("post", outer, outer_ports);

    ASSERT_EQ(desc.subgraphs.size(), 2U);
    const RendSubgraphVK& inner = desc.get_subgraph("post.sort");
    EXPECT_EQ(inner.members[0], "post.sort.register");
    ASSERT_EQ(inner.next.size(), 1U);
    EXPECT_EQ(inner.next[0].next_task_id, "post.begin");
    EXPECT_EQ(inner.next[0].res_id, "result_image");
    EXPECT_EQ(desc.get_subgraph("post").members.size(), 3U);
    const RendTaskDescVK& registering = desc.get_task("post.sort.register");
    EXPECT_EQ(registering.resource_map.resolve("input"), "result_image");
    EXPECT_EQ(registering.resource_map.resolve("keys"), "post.sort.keys");
    EXPECT_EQ(desc.buffers[0].res_id, "post.sort.keys");
}

TEST(WBERendGraphDescVKTest, SubgraphImportsMergeIntoTheGraph) {
    RendAlloc alloc(WBE_KI_B(128));
    Ref<ROCSingle> mesh = make_import_container(&alloc);
    RendGraphDescVK desc(&alloc);
    desc.add_resource(RendResVK{.res_id = "mesh", .container = mesh});
    RendSubgraphDescVK subgraph(&alloc);
    subgraph.graph.add_resource(RendResVK{.res_id = "mesh", .container = mesh});
    subgraph.graph.add_resource(RendResVK{.res_id = "camera", .container = make_import_container(&alloc)});
    subgraph.graph.add_task(make_empty_task(&alloc, "draw"));
    desc.add_subgraph("first", subgraph, {});

    ASSERT_EQ(desc.resources.size(), 2U);
    EXPECT_EQ(desc.resources[1].res_id, "camera");
    // Each import of a render object wraps it in a container of its own; the same object still merges.
    const Ref<IRendObject> camera = desc.resources[1].container->get().dynamic_cast_ref<IRendObject>();
    RendSubgraphDescVK rewrapping(&alloc);
    rewrapping.graph.add_resource(RendResVK{.res_id = "camera", .container = make_ref<ROCSingle>(&alloc, [camera](uint32_t /*p_slot*/) {
                                                return camera;
                                            })});
    desc.add_subgraph("rewrapping", rewrapping, {});
    EXPECT_EQ(desc.resources.size(), 2U);
    // A second subgraph importing another resource under an imported id conflicts.
    RendSubgraphDescVK conflicting(&alloc);
    conflicting.graph.add_resource(RendResVK{.res_id = "mesh", .container = make_import_container(&alloc)});
    EXPECT_THROW(desc.add_subgraph("second", conflicting, {}), std::runtime_error);
}

TEST(WBERendGraphDescVKTest, SubgraphsRejectInvalidPortsIdsAndEdges) {
    RendAlloc alloc(WBE_KI_B(128));
    RendGraphDescVK desc(&alloc);
    desc.add_task(make_empty_task(&alloc, "present"));
    const RendSubgraphDescVK subgraph = make_sort_subgraph(&alloc);
    const std::array<RendSubgraphPortBindingVK, 1> ports{{{.port = "color", .resource = "result_image"}}};
    const std::array<RendSubgraphPortBindingVK, 2> twice{{{.port = "color", .resource = "a"}, {.port = "color", .resource = "b"}}};
    const std::array<RendSubgraphPortBindingVK, 2> unknown{{{.port = "color", .resource = "a"}, {.port = "depth", .resource = "b"}}};
    const std::array<RendSubgraphPortBindingVK, 1> empty{{{.port = "color", .resource = ""}}};

    EXPECT_THROW(desc.add_subgraph("sort", subgraph, {}), std::runtime_error);
    EXPECT_THROW(desc.add_subgraph("sort", subgraph, twice), std::runtime_error);
    EXPECT_THROW(desc.add_subgraph("sort", subgraph, unknown), std::runtime_error);
    EXPECT_THROW(desc.add_subgraph("sort", subgraph, empty), std::runtime_error);
    EXPECT_THROW(desc.add_subgraph("", subgraph, ports), std::runtime_error);
    EXPECT_THROW(desc.add_subgraph("present", subgraph, ports), std::runtime_error);
    desc.add_subgraph("sort", subgraph, ports);
    EXPECT_THROW(desc.add_subgraph("sort", subgraph, ports), std::runtime_error);

    RendSubgraphDescVK shadowing(&alloc);
    shadowing.add_port("keys");
    shadowing.graph.add_buffer("keys", [](const RendGraphContextVK& /*p_context*/) {
        return RendBufferDescVK{};
    });
    const std::array<RendSubgraphPortBindingVK, 1> keys{{{.port = "keys", .resource = "result_image"}}};
    EXPECT_THROW(desc.add_subgraph("shadowing", shadowing, keys), std::runtime_error);

    RendSubgraphDescVK leaving(&alloc);
    leaving.graph.add_task(make_empty_task(&alloc, "inside"));
    leaving.graph.get_task("inside").connect("present", "result_image");
    EXPECT_THROW(desc.add_subgraph("leaving", leaving, {}), std::runtime_error);
    EXPECT_THROW(RendSubgraphDescVK(&alloc).add_port("a").add_port("a"), std::runtime_error);
}

} // namespace WhiteBirdEngine
