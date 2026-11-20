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

#include "core/alloc/stl_alloc.hh"
#include "core/memory/unique.hh"
#include "function/event/event_manager.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/rt_multiplexer_vk.hh"
#include "global/global.hh"
#include "test_global.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

struct MultiplexerProbeState {
    int recordings = 0;
    int bindings = 0;
    int destructions = 0;
};

class RTMultiplexerProbeVK final : public IRendTaskVK {
public:
    RTMultiplexerProbeVK() = delete;
    RTMultiplexerProbeVK(MultiplexerProbeState& p_state, std::string p_resource_id, QueueFamilyVK p_queue_family = QueueFamilyVK::COMPUTE_1)
        : state(p_state), resource_id(std::move(p_resource_id)), family(p_queue_family) {
    }
    virtual ~RTMultiplexerProbeVK() override {
        ++state.destructions;
    }
    RTMultiplexerProbeVK(const RTMultiplexerProbeVK&) = delete;
    RTMultiplexerProbeVK(RTMultiplexerProbeVK&&) = delete;
    RTMultiplexerProbeVK& operator=(const RTMultiplexerProbeVK&) = delete;
    RTMultiplexerProbeVK& operator=(RTMultiplexerProbeVK&&) = delete;
    virtual QueueFamilyVK queue_family() const override {
        return family;
    }
    virtual void append_accesses(Vector<ResAccessVK, RendAlloc>& p_reads, Vector<ResAccessVK, RendAlloc>& p_writes) const override {
        p_reads.emplace_back("input", RendResUsageVK::STORAGE_BUFFER);
        p_writes.emplace_back(resource_id, RendResUsageVK::STORAGE_IMAGE);
    }
    virtual void bind_resources(const RendResViewVK&) override {
        ++state.bindings;
    }
    virtual void record(VkCommandBuffer, double) override {
        ++state.recordings;
    }

private:
    MultiplexerProbeState& state;
    std::string resource_id;
    QueueFamilyVK family;
};

Vector<Unique<IRendTaskVK>, RendAlloc> make_branches(
    RendAlloc& p_alloc, MultiplexerProbeState& p_first, MultiplexerProbeState& p_second) {
    Vector<Unique<IRendTaskVK>, RendAlloc> branches(&p_alloc);
    branches.push_back(make_unique<RTMultiplexerProbeVK>(&p_alloc, p_first, "first_output"));
    branches.push_back(make_unique<RTMultiplexerProbeVK>(&p_alloc, p_second, "second_output"));
    return branches;
}

void record_frames(IRendTaskVK& p_task, int p_frames) {
    for (int frame = 0; frame < p_frames; ++frame) {
        p_task.record(VK_NULL_HANDLE, 0.0);
    }
}

// Event-driven tasks subscribe through the EventManager singleton, which the test global owns.
class WBERTMultiplexerVKEventTest : public ::testing::Test {
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

TEST(WBERTMultiplexerVKTest, RecordsOnlyTheSelectedBranch) {
    RendAlloc alloc(WBE_KI_B(64));
    MultiplexerProbeState first;
    MultiplexerProbeState second;
    RTMultiplexerVK multiplexer(make_branches(alloc, first, second));
    EXPECT_EQ(multiplexer.get_branch(), 0U);
    EXPECT_EQ(multiplexer.get_branch_count(), 2U);
    record_frames(multiplexer, 2);
    EXPECT_EQ(first.recordings, 2);
    EXPECT_EQ(second.recordings, 0);
    multiplexer.set_branch(1);
    record_frames(multiplexer, 3);
    EXPECT_EQ(first.recordings, 2);
    EXPECT_EQ(second.recordings, 3);
}

TEST(WBERTMultiplexerVKTest, StartsOnTheConstructedBranch) {
    RendAlloc alloc(WBE_KI_B(64));
    MultiplexerProbeState first;
    MultiplexerProbeState second;
    RTMultiplexerVK multiplexer(make_branches(alloc, first, second), 1U);
    record_frames(multiplexer, 1);
    EXPECT_EQ(first.recordings, 0);
    EXPECT_EQ(second.recordings, 1);
}

TEST(WBERTMultiplexerVKTest, ReportsAndBindsEveryBranchAndOwnsThem) {
    RendAlloc alloc(WBE_KI_B(64));
    MultiplexerProbeState first;
    MultiplexerProbeState second;
    {
        RTMultiplexerVK multiplexer(make_branches(alloc, first, second));
        EXPECT_EQ(multiplexer.queue_family(), QueueFamilyVK::COMPUTE_1);
        Vector<ResAccessVK, RendAlloc> reads(&alloc);
        Vector<ResAccessVK, RendAlloc> writes(&alloc);
        multiplexer.append_accesses(reads, writes);
        // The unselected branch's accesses are reported too, so the graph orders the node for either branch.
        EXPECT_EQ(reads.size(), 2U);
        ASSERT_EQ(writes.size(), 2U);
        EXPECT_EQ(writes[0].res_id, "first_output");
        EXPECT_EQ(writes[1].res_id, "second_output");
        Map<std::string, Ref<RendResVK>, RendAlloc> resources(&alloc);
        multiplexer.bind_resources(RendResViewVK(&resources));
        EXPECT_EQ(first.bindings, 1);
        EXPECT_EQ(second.bindings, 1);
    }
    EXPECT_EQ(first.destructions, 1);
    EXPECT_EQ(second.destructions, 1);
}

TEST(WBERTMultiplexerVKTest, RejectsAnOutOfRangeBranch) {
    RendAlloc alloc(WBE_KI_B(64));
    MultiplexerProbeState first;
    MultiplexerProbeState second;
    EXPECT_THROW(RTMultiplexerVK(make_branches(alloc, first, second), 2U), std::runtime_error);
    RTMultiplexerVK multiplexer(make_branches(alloc, first, second));
    EXPECT_THROW(multiplexer.set_branch(2), std::runtime_error);
    EXPECT_EQ(multiplexer.get_branch(), 0U);
}

TEST(WBERTMultiplexerVKTest, RejectsInvalidBranches) {
    RendAlloc alloc(WBE_KI_B(64));
    MultiplexerProbeState state;
    EXPECT_THROW(RTMultiplexerVK(Vector<Unique<IRendTaskVK>, RendAlloc>(&alloc)), std::runtime_error);
    Vector<Unique<IRendTaskVK>, RendAlloc> with_null(&alloc);
    with_null.push_back(make_unique<RTMultiplexerProbeVK>(&alloc, state, "output"));
    with_null.emplace_back();
    EXPECT_THROW(RTMultiplexerVK(std::move(with_null)), std::runtime_error);
    Vector<Unique<IRendTaskVK>, RendAlloc> mixed_queues(&alloc);
    mixed_queues.push_back(make_unique<RTMultiplexerProbeVK>(&alloc, state, "output"));
    mixed_queues.push_back(make_unique<RTMultiplexerProbeVK>(&alloc, state, "output", QueueFamilyVK::GRAPHICS));
    EXPECT_THROW(RTMultiplexerVK(std::move(mixed_queues)), std::runtime_error);
}

TEST_F(WBERTMultiplexerVKEventTest, CommandsSelectTheBranchOfEveryTaskSharingTheId) {
    RendAlloc alloc(WBE_KI_B(64));
    MultiplexerProbeState first;
    MultiplexerProbeState second;
    MultiplexerProbeState third;
    MultiplexerProbeState fourth;
    RTMultiplexerVK multiplexer(make_branches(alloc, first, second), "probe.select");
    RTMultiplexerVK other(make_branches(alloc, third, fourth), "probe.select");
    EventManager::get_singleton()->emit("probe.select", RTMultiplexerVK::Command{.branch = 1});
    EXPECT_EQ(multiplexer.get_branch(), 1U);
    EXPECT_EQ(other.get_branch(), 1U);
    record_frames(multiplexer, 1);
    EXPECT_EQ(second.recordings, 1);
    EXPECT_THROW(EventManager::get_singleton()->emit("probe.select", RTMultiplexerVK::Command{.branch = 2}), std::runtime_error);
}

TEST_F(WBERTMultiplexerVKEventTest, DestroyingTheTaskEndsTheSubscription) {
    RendAlloc alloc(WBE_KI_B(64));
    MultiplexerProbeState first;
    MultiplexerProbeState second;
    {
        RTMultiplexerVK multiplexer(make_branches(alloc, first, second), "probe.destroyed");
        EXPECT_EQ(EventManager::get_singleton()->get_subscriber_count<RTMultiplexerVK::Command>("probe.destroyed"), 1U);
    }
    EXPECT_EQ(EventManager::get_singleton()->get_subscriber_count<RTMultiplexerVK::Command>("probe.destroyed"), 0U);
    EXPECT_THROW(RTMultiplexerVK(make_branches(alloc, first, second), std::string_view()), std::runtime_error);
}

TEST_F(WBERTMultiplexerVKEventTest, CombinedFactoriesCreateMultiplexersOnTheGivenBranch) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    MultiplexerProbeState first;
    MultiplexerProbeState second;
    Vector<RendTaskFactoryVK, RendAlloc> factories(&alloc);
    factories.push_back([&](const RendGraphContextVK&) -> Unique<IRendTaskVK> {
        return make_unique<RTMultiplexerProbeVK>(&alloc, first, "first_output");
    });
    factories.push_back([&](const RendGraphContextVK&) -> Unique<IRendTaskVK> {
        return make_unique<RTMultiplexerProbeVK>(&alloc, second, "second_output");
    });
    const RendTaskFactoryVK factory = RTMultiplexerVK::combine_factories(factories, "probe.combined", &rend_data, 1);
    Unique<IRendTaskVK> task = factory(RendGraphContextVK{});
    auto* multiplexer = dynamic_cast<RTMultiplexerVK*>(task.get());
    ASSERT_NE(multiplexer, nullptr);
    EXPECT_EQ(multiplexer->get_branch(), 1U);
    EventManager::get_singleton()->emit("probe.combined", RTMultiplexerVK::Command{.branch = 0});
    EXPECT_EQ(multiplexer->get_branch(), 0U);
    // A rebuild evaluates the factory again; the new multiplexer starts on the combined branch, not the last commanded one.
    Unique<IRendTaskVK> rebuilt = factory(RendGraphContextVK{});
    EXPECT_EQ(dynamic_cast<RTMultiplexerVK*>(rebuilt.get())->get_branch(), 1U);
}

TEST_F(WBERTMultiplexerVKEventTest, CombineFactoriesRejectsInvalidInputs) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    MultiplexerProbeState state;
    Vector<RendTaskFactoryVK, RendAlloc> factories(&alloc);
    factories.push_back([&](const RendGraphContextVK&) -> Unique<IRendTaskVK> {
        return make_unique<RTMultiplexerProbeVK>(&alloc, state, "output");
    });
    EXPECT_THROW(RTMultiplexerVK::combine_factories(Vector<RendTaskFactoryVK, RendAlloc>(&alloc), "probe.invalid", &rend_data),
        std::runtime_error);
    EXPECT_THROW(RTMultiplexerVK::combine_factories(factories, "probe.invalid", &rend_data, 1), std::runtime_error);
    EXPECT_THROW(RTMultiplexerVK::combine_factories(factories, "", &rend_data), std::runtime_error);
    EXPECT_THROW(RTMultiplexerVK::combine_factories(factories, "probe.invalid", nullptr), std::runtime_error);
    Vector<RendTaskFactoryVK, RendAlloc> with_empty(&alloc);
    with_empty.emplace_back();
    EXPECT_THROW(RTMultiplexerVK::combine_factories(with_empty, "probe.invalid", &rend_data), std::runtime_error);
}

// Runs without a test global, so no EventManager exists.
TEST(WBERTMultiplexerVKStandaloneTest, EventModeRequiresEventManager) {
    RendAlloc alloc(WBE_KI_B(64));
    MultiplexerProbeState first;
    MultiplexerProbeState second;
    ASSERT_EQ(EventManager::get_singleton(), nullptr);
    EXPECT_THROW(RTMultiplexerVK(make_branches(alloc, first, second), "probe.no_manager"), std::runtime_error);
}

} // namespace WhiteBirdEngine
