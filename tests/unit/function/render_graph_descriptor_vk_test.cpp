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
#include "core/allocator/heap_allocator_aligned_pool_impl_list.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include <gtest/gtest.h>
#include <stdexcept>

namespace WhiteBirdEngine {

/** @brief Minimal render task used to verify direct descriptor ownership transfer. */
class DirectDescriptorRenderTaskVK final : public RenderTaskVK {
public:
    DirectDescriptorRenderTaskVK(RenderAllocator* p_allocator, bool* p_destroyed)
        : allocator(p_allocator), destroyed(p_destroyed) {
    }
    virtual ~DirectDescriptorRenderTaskVK() override {
        *destroyed = true;
    }
    DirectDescriptorRenderTaskVK(const DirectDescriptorRenderTaskVK&) = delete;
    DirectDescriptorRenderTaskVK(DirectDescriptorRenderTaskVK&&) noexcept = delete;
    DirectDescriptorRenderTaskVK& operator=(const DirectDescriptorRenderTaskVK&) = delete;
    DirectDescriptorRenderTaskVK& operator=(DirectDescriptorRenderTaskVK&&) noexcept = delete;

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }

    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        return Vector<ResAccessVK, RenderAllocator>(allocator);
    }

    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        return Vector<ResAccessVK, RenderAllocator>(allocator);
    }

    virtual void record(VkCommandBuffer /*p_command_buffer*/, const RendResViewVK& /*p_resources*/, double /*p_delta*/) override {
    }

private:
    RenderAllocator* allocator;
    bool* destroyed;
};

TEST(WBERenderGraphDescriptorVKTest, DescriptorCopiesShareOneDirectTaskTransfer) {
    RenderAllocator allocator(WBE_KI_B(64));
    bool destroyed = false;
    RendTaskDescVK descriptor(&allocator, 17, make_unique<DirectDescriptorRenderTaskVK>(&allocator, &allocator, &destroyed));
    RendTaskDescVK descriptor_copy = descriptor;

    Unique<RenderTaskVK> task = descriptor_copy.create();

    EXPECT_FALSE(task.is_null());
    EXPECT_EQ(descriptor.task_id, 17U);
    EXPECT_THROW(descriptor.create(), std::runtime_error);
    EXPECT_THROW(descriptor_copy.create(), std::runtime_error);
    EXPECT_FALSE(destroyed);
    task.reset();
    EXPECT_TRUE(destroyed);
}

} // namespace WhiteBirdEngine
