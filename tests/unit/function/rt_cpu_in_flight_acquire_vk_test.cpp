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
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/rt_cpu_in_flight_acquire_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "utils/defs.hh"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <semaphore>
#include <span>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TEST(WBERTCPUInFlightAcquireVKTest, DirectRecordRequiresBoundSource) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    std::array<uint8_t, 4> destination{};
    std::binary_semaphore completion_semaphore(0);
    RTCPUInFlightAcquireVK task(&render_data, "source_image", destination, &completion_semaphore);

    ASSERT_EQ(task.queue_family(), QueueFamilyVK::GRAPHICS);
    Vector<ResAccessVK, RenderAllocator> reads(&allocator);
    Vector<ResAccessVK, RenderAllocator> writes(&allocator);
    task.append_accesses(reads, writes);
    ASSERT_EQ(reads.size(), 1U);
    EXPECT_EQ(reads[0].res_id, "source_image");
    EXPECT_EQ(reads[0].usage, RendResUsageVK::TRANSFER_SRC);
    EXPECT_TRUE(writes.empty());

    EXPECT_THROW(task.record(VK_NULL_HANDLE, 0.0), std::runtime_error);
    EXPECT_FALSE(completion_semaphore.try_acquire());
}

TEST(WBERTCPUInFlightAcquireVKTest, RejectsInvalidNonOwningInputs) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    std::array<uint8_t, 4> destination{};
    std::binary_semaphore completion_semaphore(0);

    EXPECT_THROW(RTCPUInFlightAcquireVK(nullptr, "source_image", destination, &completion_semaphore), std::runtime_error);
    EXPECT_THROW(RTCPUInFlightAcquireVK(&render_data, "", destination, &completion_semaphore), std::runtime_error);
    EXPECT_THROW(RTCPUInFlightAcquireVK(&render_data, "source_image", std::span<uint8_t>(), &completion_semaphore), std::runtime_error);
    EXPECT_THROW(RTCPUInFlightAcquireVK(&render_data, "source_image", destination, nullptr), std::runtime_error);
}

TEST(WBERTCPUInFlightAcquireVKTest, TriggeredAcquireSkipsUnboundSourceUntilActivated) {
    RenderAllocator allocator(WBE_KI_B(64));
    BaseRenderDataVK render_data(&allocator);
    std::array<uint8_t, 4> destination{};
    std::binary_semaphore completion_semaphore(0);
    RTTriggeredVK task(make_unique<RTCPUInFlightAcquireVK>(&allocator, &render_data, "source_image", destination, &completion_semaphore));
    EXPECT_NO_THROW(task.record(VK_NULL_HANDLE, 0.0));
    task.activate.store(true);
    EXPECT_THROW(task.record(VK_NULL_HANDLE, 0.0), std::runtime_error);
    EXPECT_FALSE(task.activate.load());
    EXPECT_FALSE(completion_semaphore.try_acquire());
}

} // namespace WhiteBirdEngine
