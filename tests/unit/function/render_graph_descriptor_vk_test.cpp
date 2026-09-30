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
#include "function/render/render_resource_list.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/** @brief Minimal render task used to verify direct descriptor ownership transfer. */
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

TEST(WBERenderGraphDescriptorVKTest, DescriptorCopiesShareOneDirectTaskTransfer) {
    RenderAllocator allocator(WBE_KI_B(64));
    bool destroyed = false;
    RendTaskDescVK descriptor(&allocator, "direct_task", make_unique<RTDirectDescriptorVK>(&allocator, &destroyed));
    const RendTaskDescVK& descriptor_copy = descriptor;

    Unique<IRenderTaskVK> task = descriptor_copy.create();

    EXPECT_FALSE(task.is_null());
    EXPECT_EQ(descriptor.task_id, "direct_task");
    EXPECT_THROW(descriptor.create(), std::runtime_error);
    EXPECT_THROW(descriptor_copy.create(), std::runtime_error);
    EXPECT_FALSE(destroyed);
    task.reset();
    EXPECT_TRUE(destroyed);
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
    auto resource_list = make_ref<RenderResourceList>(&allocator, &allocator);

    descriptor.add_resource("textures", resource_list);

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
            {.usage = RendResPrimaryUsageVK::BUFFER_UNIFORM, .domain = DeviceDomain::HOST_DEVICE, .per_frame = true, .initial_data = {1, 2}})
        .add_image("albedo_image",
            {.usage = RendResPrimaryUsageVK::IMAGE_RENDER_TARGET,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .extent = {.width = 8, .height = 8, .depth = 1},
                .mip_levels = 2})
        .add_image_view("albedo", "albedo_image", {.mip_level_count = VK_REMAINING_MIP_LEVELS})
        .add_sampler("sampler", {.min_filter = VK_FILTER_NEAREST, .max_anisotropy = 4.0F});

    ASSERT_EQ(descriptor.buffers.size(), 1U);
    EXPECT_EQ(descriptor.buffers.front().res_id, "camera");
    EXPECT_TRUE(descriptor.buffers.front().desc.per_frame);
    EXPECT_EQ(descriptor.buffers.front().desc.initial_data.size(), 2U);
    ASSERT_EQ(descriptor.images.size(), 1U);
    EXPECT_EQ(descriptor.images.front().desc.mip_levels, 2U);
    ASSERT_EQ(descriptor.image_views.size(), 1U);
    EXPECT_EQ(descriptor.image_views.front().image_id, "albedo_image");
    EXPECT_EQ(descriptor.image_views.front().desc.mip_level_count, VK_REMAINING_MIP_LEVELS);
    ASSERT_EQ(descriptor.samplers.size(), 1U);
    EXPECT_EQ(descriptor.samplers.front().desc.min_filter, VK_FILTER_NEAREST);
    EXPECT_EQ(descriptor.samplers.front().desc.mag_filter, VK_FILTER_LINEAR);
    EXPECT_EQ(descriptor.samplers.front().desc.max_anisotropy, 4.0F);
    EXPECT_FALSE(descriptor.samplers.front().desc.compare_op.has_value());
    EXPECT_TRUE(descriptor.resources.empty());
}

TEST(WBERenderGraphDescriptorVKTest, GraphOwnedResourceDeclarationsRejectEmptyIds) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);

    EXPECT_THROW(descriptor.add_buffer("", {}), std::runtime_error);
    EXPECT_THROW(descriptor.add_image("", {}), std::runtime_error);
    EXPECT_THROW(descriptor.add_image_view("view", ""), std::runtime_error);
    EXPECT_THROW(descriptor.add_sampler(""), std::runtime_error);
    EXPECT_THROW(descriptor.add_static_set(MEM_NULL), std::runtime_error);
}

} // namespace WhiteBirdEngine
