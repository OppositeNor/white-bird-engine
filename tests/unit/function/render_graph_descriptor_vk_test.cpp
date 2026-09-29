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
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/** @brief Minimal render task used to verify direct descriptor ownership transfer. */
class RTDirectDescriptorVK final : public IRenderTaskVK {
public:
    RTDirectDescriptorVK(RenderAllocator* p_allocator, bool* p_destroyed) : allocator(p_allocator), destroyed(p_destroyed) {
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

    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        return Vector<ResAccessVK, RenderAllocator>(allocator);
    }

    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        return Vector<ResAccessVK, RenderAllocator>(allocator);
    }

    virtual void record(VkCommandBuffer /*p_command_buffer*/, double /*p_delta*/) override {
    }

private:
    RenderAllocator* allocator;
    bool* destroyed;
};

TEST(WBERenderGraphDescriptorVKTest, DescriptorCopiesShareOneDirectTaskTransfer) {
    RenderAllocator allocator(WBE_KI_B(64));
    bool destroyed = false;
    RendTaskDescVK descriptor(&allocator, "direct_task", make_unique<RTDirectDescriptorVK>(&allocator, &allocator, &destroyed));
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

TEST(WBERenderGraphDescriptorVKTest, FluentResourceDeclarationsUseStringsAndCompactUsages) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendTaskDescVK descriptor(&allocator);
    descriptor.task_id = "run_lighting";
    descriptor.read("gbuffer_albedo", RendResUsageVK::SAMPLED_IMAGE)
        .write("lighting_output", RendResUsageVK::STORAGE_IMAGE)
        .connect("run_bloom",
            "lighting_output",
            RendEdgeTypeVK::READ,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    ASSERT_EQ(descriptor.next.size(), 1U);
    EXPECT_EQ(descriptor.next.front().next_task_id, "run_bloom");
    EXPECT_EQ(descriptor.next.front().res_id, "lighting_output");
    EXPECT_EQ(descriptor.next.front().type, RendEdgeTypeVK::READ);
    EXPECT_EQ(descriptor.next.front().src_stage, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    EXPECT_EQ(descriptor.next.front().dst_stage, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    ASSERT_EQ(descriptor.reads.size(), 1U);
    EXPECT_EQ(descriptor.reads.front().res_id, "gbuffer_albedo");
    EXPECT_EQ(descriptor.reads.front().usage, RendResUsageVK::SAMPLED_IMAGE);
    ASSERT_EQ(descriptor.writes.size(), 1U);
    EXPECT_EQ(descriptor.writes.front().res_id, "lighting_output");
    EXPECT_EQ(descriptor.writes.front().usage, RendResUsageVK::STORAGE_IMAGE);
}

TEST(WBERenderGraphDescriptorVKTest, GetTaskReturnsStoredDescriptorForLaterConnections) {
    RenderAllocator allocator(WBE_KI_B(64));
    RendGraphDescVK descriptor(&allocator);
    RendTaskDescVK task(&allocator);
    task.task_id = "producer";
    descriptor.add_task(std::move(task));

    descriptor.get_task("producer").connect(
        "consumer", "buffer", RendEdgeTypeVK::WRITE, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    ASSERT_EQ(descriptor.tasks.front().next.size(), 1U);
    EXPECT_EQ(descriptor.tasks.front().next.front().type, RendEdgeTypeVK::WRITE);
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

} // namespace WhiteBirdEngine
