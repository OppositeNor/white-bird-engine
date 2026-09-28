/* Copyright 2026 OppositeNor

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
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "utils/defs.hh"
#include <functional>
#include <gtest/gtest.h>
#include <stdexcept>

namespace WhiteBirdEngine {
namespace {

struct TriggerProbeState {
    int recordings = 0;
    int initializations = 0;
    int bindings = 0;
    int destructions = 0;
    double delta = 0.0;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    std::function<void()> on_record;
};

class RTTriggerProbeVK final : public IRenderTaskVK {
public:
    RTTriggerProbeVK() = delete;
    RTTriggerProbeVK(RenderAllocator* p_allocator, TriggerProbeState& p_state) : allocator(p_allocator), state(p_state) {
    }
    ~RTTriggerProbeVK() override {
        ++state.destructions;
    }
    RTTriggerProbeVK(const RTTriggerProbeVK&) = delete;
    RTTriggerProbeVK(RTTriggerProbeVK&&) = delete;
    RTTriggerProbeVK& operator=(const RTTriggerProbeVK&) = delete;
    RTTriggerProbeVK& operator=(RTTriggerProbeVK&&) = delete;
    QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::COMPUTE;
    }
    Vector<ResAccessVK, RenderAllocator> read_resources() const override {
        Vector<ResAccessVK, RenderAllocator> result(allocator);
        result.push_back({"input", RendResUsageVK::STORAGE_BUFFER});
        return result;
    }
    Vector<ResAccessVK, RenderAllocator> write_resources() const override {
        Vector<ResAccessVK, RenderAllocator> result(allocator);
        result.push_back({"output", RendResUsageVK::STORAGE_IMAGE});
        return result;
    }
    void bind_resources(const RendResViewVK&) override {
        ++state.bindings;
    }
    void init() override {
        ++state.initializations;
    }
    void record(VkCommandBuffer p_command_buffer, double p_delta) override {
        ++state.recordings;
        state.command_buffer = p_command_buffer;
        state.delta = p_delta;
        if (state.on_record) {
            state.on_record();
        }
    }

private:
    RenderAllocator* allocator;
    TriggerProbeState& state;
};

} // namespace

TEST(WBERTTriggeredVKTest, ConsumesOneActivationAndCanBeRearmed) {
    RenderAllocator allocator(WBE_KI_B(64));
    TriggerProbeState state;
    RTTriggeredVK task(make_unique<RTTriggerProbeVK>(&allocator, &allocator, state));
    task.record(VK_NULL_HANDLE, 1.0);
    EXPECT_EQ(state.recordings, 0);
    task.activate.store(true);
    task.activate.store(true); // Requests coalesce.
    task.record(VK_NULL_HANDLE, 2.0);
    EXPECT_EQ(state.recordings, 1);
    EXPECT_DOUBLE_EQ(state.delta, 2.0);
    EXPECT_EQ(state.command_buffer, VK_NULL_HANDLE);
    EXPECT_FALSE(task.activate.load());
    task.record(VK_NULL_HANDLE, 3.0);
    EXPECT_EQ(state.recordings, 1);
    task.activate.store(true);
    task.record(VK_NULL_HANDLE, 4.0);
    EXPECT_EQ(state.recordings, 2);
}

TEST(WBERTTriggeredVKTest, ActivationDuringRecordingRemainsPending) {
    RenderAllocator allocator(WBE_KI_B(64));
    TriggerProbeState state;
    RTTriggeredVK task(make_unique<RTTriggerProbeVK>(&allocator, &allocator, state), true);
    state.on_record = [&task]() {
        task.activate.store(true);
    };
    task.record(VK_NULL_HANDLE, 0.0);
    EXPECT_TRUE(task.activate.load());
    state.on_record = {};
    task.record(VK_NULL_HANDLE, 0.0);
    EXPECT_EQ(state.recordings, 2);
    EXPECT_FALSE(task.activate.load());
}

TEST(WBERTTriggeredVKTest, InactiveWrapperForwardsSetupAndOwnsChild) {
    RenderAllocator allocator(WBE_KI_B(64));
    TriggerProbeState state;
    {
        RTTriggeredVK task(make_unique<RTTriggerProbeVK>(&allocator, &allocator, state));
        Map<std::string, Ref<RendResVK>, RenderAllocator> resources(&allocator);
        task.init();
        task.bind_resources(RendResViewVK(&resources));
        EXPECT_EQ(state.initializations, 1);
        EXPECT_EQ(state.bindings, 1);
        EXPECT_EQ(task.queue_family(), QueueFamilyVK::COMPUTE);
        const auto reads = task.read_resources();
        const auto writes = task.write_resources();
        ASSERT_EQ(reads.size(), 1U);
        ASSERT_EQ(writes.size(), 1U);
        EXPECT_EQ(reads[0].res_id, "input");
        EXPECT_EQ(reads[0].usage, RendResUsageVK::STORAGE_BUFFER);
        EXPECT_EQ(writes[0].res_id, "output");
        EXPECT_EQ(writes[0].usage, RendResUsageVK::STORAGE_IMAGE);
        EXPECT_EQ(state.recordings, 0);
        EXPECT_EQ(state.destructions, 0);
    }
    EXPECT_EQ(state.destructions, 1);
}

TEST(WBERTTriggeredVKTest, RejectsNullChild) {
    EXPECT_THROW((RTTriggeredVK{Unique<IRenderTaskVK>{}}), std::runtime_error);
}

} // namespace WhiteBirdEngine
