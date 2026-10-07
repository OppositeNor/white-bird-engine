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
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/function_utils.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_bindable_render_resource_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_resource_list_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/** @brief Minimal render task used to verify that task factories create and destroy tasks. */
class RTDirectDescriptorVK final : public IRenderTaskVK {
public:
    explicit RTDirectDescriptorVK(bool* p_destroyed) : destroyed(p_destroyed) {
    }
    virtual ~RTDirectDescriptorVK() override {
        *destroyed = true;
    }
    RTDirectDescriptorVK(const RTDirectDescriptorVK&) = delete;
    RTDirectDescriptorVK(RTDirectDescriptorVK&&) noexcept = delete;
    RTDirectDescriptorVK& operator=(const RTDirectDescriptorVK&) = delete;
    RTDirectDescriptorVK& operator=(RTDirectDescriptorVK&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& /*p_reads*/,
        Vector<ResAccessVK, RenderAllocator>& /*p_writes*/) const override {
    }

    virtual void record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) override {
    }

private:
    bool* destroyed;
};

/** @brief Minimal render object that fills the container used to test container binding. */
class TestContainerResourceVK final : public IRenderObjectVK {
public:
    explicit TestContainerResourceVK(const BaseRenderDataVK* p_render_data) : IRenderObjectVK(p_render_data) {
    }
    virtual ~TestContainerResourceVK() override = default;
    TestContainerResourceVK(const TestContainerResourceVK&) = delete;
    TestContainerResourceVK(TestContainerResourceVK&&) noexcept = delete;
    TestContainerResourceVK& operator=(const TestContainerResourceVK&) = delete;
    TestContainerResourceVK& operator=(TestContainerResourceVK&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return TypeUUID<ROBufferVK>::UUID;
    }
};

TEST(WBERenderGraphDescriptorVKTest, DescriptorStoresTaskFactory) {
    RenderAllocator allocator(WBE_KI_B(64));
    bool destroyed = false;
    int created_count = 0;
    RendTaskDescVK descriptor(&allocator, "direct_task", [&allocator, &destroyed, &created_count](const RendGraphContextVK& /*p_context*/) {
        ++created_count;
        return make_unique<RTDirectDescriptorVK>(&allocator, &destroyed);
    });
    RendTaskDescVK copied_descriptor = descriptor;

    EXPECT_EQ(copied_descriptor.task_id, "direct_task");
    ASSERT_NE(copied_descriptor.factory, nullptr);
    EXPECT_EQ(created_count, 0);

    Unique<IRenderTaskVK> task = copied_descriptor.factory(RendGraphContextVK{});

    EXPECT_FALSE(task.is_null());
    EXPECT_EQ(created_count, 1);
    EXPECT_FALSE(destroyed);
    task.reset();
    EXPECT_TRUE(destroyed);
}

TEST(WBERenderGraphDescriptorVKTest, TaskFactoryConstructorRejectsNullFactory) {
    RenderAllocator allocator(WBE_KI_B(64));

    EXPECT_THROW(RendTaskDescVK(&allocator, "null_task", nullptr), std::runtime_error);
}

TEST(WBERenderGraphDescriptorVKTest, FluentAliasesAndConnectionsAreStored) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendTaskDescVK descriptor(&allocator);
    descriptor.task_id = "run_lighting";
    descriptor.alias("render_target_colors[0]", "lighting_output").connect("run_bloom", "lighting_output");

    ASSERT_EQ(descriptor.next.size(), 1U);
    EXPECT_EQ(descriptor.next.front().next_task_id, "run_bloom");
    EXPECT_EQ(descriptor.next.front().res_id, "lighting_output");
    EXPECT_EQ(descriptor.resource_map.resolve("render_target_colors[0]"), "lighting_output");
    EXPECT_EQ(descriptor.resource_map.resolve("gbuffer_albedo"), "gbuffer_albedo");
    EXPECT_THROW(descriptor.alias("render_target_colors[0]", "other"), std::runtime_error);
}

TEST(WBERenderGraphDescriptorVKTest, GetTaskReturnsStoredDescriptorForLaterConnections) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);
    RendTaskDescVK task(&allocator);
    task.task_id = "producer";
    descriptor.add_task(std::move(task));

    descriptor.get_task("producer").connect("consumer", "buffer");

    ASSERT_EQ(descriptor.tasks.front().next.size(), 1U);
    EXPECT_EQ(descriptor.tasks.front().next.front().next_task_id, "consumer");
    EXPECT_EQ(descriptor.tasks.front().next.front().res_id, "buffer");
    EXPECT_THROW(descriptor.get_task("missing"), std::runtime_error);
}

TEST(WBERenderGraphDescriptorVKTest, ResourceListIsStoredAsAnAggregateBinding) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);
    auto resource_list = make_ref<RenderResourceListVK>(&allocator, &allocator);

    descriptor.add_resource("textures", *resource_list);

    ASSERT_EQ(descriptor.resources.size(), 1U);
    EXPECT_EQ(descriptor.resources.front().res_id, "textures");
    EXPECT_EQ(descriptor.resources.front().resource_list, resource_list);
    EXPECT_EQ(descriptor.resources.front().container, MEM_NULL);
}

TEST(WBERenderGraphDescriptorVKTest, GraphOwnedResourceDeclarationsAreStoredForCompilation) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);

    descriptor
        .add_buffer("camera",
            [=](const RendGraphContextVK& /*p_context*/) {
                return RendBufferDescVK{.usage = RendResPrimaryUsageVK::BUFFER_UNIFORM,
                    .domain = DeviceDomain::HOST_DEVICE,
                    .per_frame = true,
                    .initial_data = {1, 2}};
            })
        .add_image("albedo_image",
            [=](const RendGraphContextVK& /*p_context*/) {
                return RendImageDescVK{.usage = RendResPrimaryUsageVK::IMAGE_RENDER_TARGET,
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

    ASSERT_EQ(descriptor.buffers.size(), 1U);
    EXPECT_EQ(descriptor.buffers.front().res_id, "camera");
    EXPECT_TRUE(descriptor.buffers.front().factory(RendGraphContextVK{}).per_frame);
    EXPECT_EQ(descriptor.buffers.front().factory(RendGraphContextVK{}).initial_data.size(), 2U);
    ASSERT_EQ(descriptor.images.size(), 1U);
    EXPECT_EQ(descriptor.images.front().factory(RendGraphContextVK{}).mip_levels, 2U);
    ASSERT_EQ(descriptor.image_views.size(), 1U);
    EXPECT_EQ(descriptor.image_views.front().image_id, "albedo_image");
    EXPECT_EQ(descriptor.image_views.front().factory(RendGraphContextVK{}).mip_level_count, VK_REMAINING_MIP_LEVELS);
    ASSERT_EQ(descriptor.samplers.size(), 1U);
    EXPECT_EQ(descriptor.samplers.front().factory(RendGraphContextVK{}).min_filter, VK_FILTER_NEAREST);
    EXPECT_EQ(descriptor.samplers.front().factory(RendGraphContextVK{}).mag_filter, VK_FILTER_LINEAR);
    EXPECT_EQ(descriptor.samplers.front().factory(RendGraphContextVK{}).max_anisotropy, 4.0F);
    EXPECT_FALSE(descriptor.samplers.front().factory(RendGraphContextVK{}).compare_op.has_value());
    EXPECT_TRUE(descriptor.resources.empty());
}

TEST(WBERenderGraphDescriptorVKTest, GraphOwnedResourceDeclarationsRejectEmptyIds) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);

    EXPECT_THROW(descriptor.add_buffer("",
                     [=](const RendGraphContextVK& /*p_context*/) {
                         return RendBufferDescVK{};
                     }),
        std::runtime_error);
    EXPECT_THROW(descriptor.add_image("",
                     [=](const RendGraphContextVK& /*p_context*/) {
                         return RendImageDescVK{};
                     }),
        std::runtime_error);
    EXPECT_THROW(descriptor.add_image_view("view", ""), std::runtime_error);
    EXPECT_THROW(descriptor.add_sampler(""), std::runtime_error);
}

TEST(WBERenderGraphDescriptorVKTest, GraphOwnedResourceDeclarationsRejectNullFactories) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);

    EXPECT_THROW(descriptor.add_buffer("buffer", nullptr), std::runtime_error);
    EXPECT_THROW(descriptor.add_image("image", nullptr), std::runtime_error);
}

TEST(WBERenderGraphDescriptorVKTest, GraphOwnedResourceFactoriesReceiveTheContext) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);

    descriptor.add_image("image", [](const RendGraphContextVK& p_context) {
        return RendImageDescVK{.format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {.width = p_context.swap_chain_extent.width, .height = p_context.swap_chain_extent.height, .depth = 1}};
    });
    descriptor.add_image_view("view", "image");
    descriptor.add_sampler("sampler");

    const RendImageDescVK image_desc =
        descriptor.images.front().factory(RendGraphContextVK{.swap_chain_extent = {.width = 640, .height = 360}});
    EXPECT_EQ(image_desc.extent.width, 640U);
    EXPECT_EQ(image_desc.extent.height, 360U);
    EXPECT_TRUE(descriptor.image_views.front().factory(RendGraphContextVK{}) == RendImageViewDescVK{});
    EXPECT_TRUE(descriptor.samplers.front().factory(RendGraphContextVK{}) == RendSamplerDescVK{});
}

TEST(WBERenderGraphDescriptorVKTest, ComposesMemberResourceIds) {
    EXPECT_EQ(IBindableRenderResourceVK::make_member_resource_id("sponza", MeshResourcesVK::NODES), "sponza.nodes");
}

TEST(WBERenderGraphDescriptorVKTest, IncompleteMeshResourcesRefuseToBind) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);
    MeshResourcesVK static_mesh(&allocator);

    EXPECT_THROW(descriptor.add_resource("static_mesh", static_mesh), std::runtime_error);
    EXPECT_TRUE(descriptor.resources.empty());
}

TEST(WBERenderGraphDescriptorVKTest, ContainerNotOwnedByRefRefusesToBindItself) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);
    BaseRenderDataVK render_data(&allocator);
    ROCMultislotsVK container(&allocator, 1, [&allocator, &render_data](uint32_t /*p_slot*/) {
        return make_ref<TestContainerResourceVK>(&allocator, &render_data).dynamic_cast_ref<IRenderObjectVK>();
    });

    EXPECT_THROW(descriptor.add_resource("container", container), std::runtime_error);
    EXPECT_TRUE(descriptor.resources.empty());
}

} // namespace WhiteBirdEngine
