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

#ifndef WBE_FILE_RT_MULTIPLEXER_VK_HH
#define WBE_FILE_RT_MULTIPLEXER_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/unique.hh"
#include "function/event/event_registration.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTMultiplexerVK
 * @brief Owns several branch tasks and records only the selected one on every frame. Neither copyable nor movable.
 * @details The multiplexer is one graph node: it reports the union of every branch's accesses, binds the resources of
 * every branch, and runs on the queue all branches share, so the graph orders and synchronizes it for whichever branch is
 * selected. Branches share the node's resource map, so a task-local id means the same graph resource in every branch.
 * A task constructed with an event id is controlled through the EventManager: emit a Command under that id, from any
 * thread, instead of holding the task. Several tasks may share one id, so one command selects their branches together;
 * emit from the rendering thread between frames so they observe it on the same recording. A recreated task, for example
 * after a graph rebuild, starts on the branch it is constructed with, so pass the current branch when wrapping.
 * set_branch() and get_branch() are atomic and may also be called from any thread. Every other method follows the
 * render-thread confinement of render tasks.
 */
class RTMultiplexerVK final : public IRenderTaskVK {
public:
    /**
     * @struct Command
     * @brief Event payload that selects the branch of every RTMultiplexerVK subscribed to the event id it is emitted
     * under. Copyable.
     */
    struct Command {
        /**
         * @brief Index of the branch the tasks record; must be below each task's branch count.
         */
        uint32_t branch = 0;
    };

    RTMultiplexerVK() = delete;
    virtual ~RTMultiplexerVK() override = default;
    RTMultiplexerVK(const RTMultiplexerVK&) = delete;
    RTMultiplexerVK(RTMultiplexerVK&&) noexcept = delete;
    RTMultiplexerVK& operator=(const RTMultiplexerVK&) = delete;
    RTMultiplexerVK& operator=(RTMultiplexerVK&&) noexcept = delete;

    /**
     * @brief Take ownership of the branches.
     * @param p_branches Non-null tasks owned exclusively by this multiplexer, indexed by branch; at least one, all on one
     * queue family.
     * @param p_branch The branch recorded until the selection is changed.
     * @throws std::runtime_error If p_branches is empty, holds a null task or tasks on different queue families, or
     * p_branch is out of range.
     */
    explicit RTMultiplexerVK(Vector<Unique<IRenderTaskVK>, RenderAllocator> p_branches, uint32_t p_branch = 0);

    /**
     * @brief Take ownership of the branches and subscribe to Command events under p_event_id.
     * The subscription ends when the task is destroyed.
     * @param p_branches As for the other constructor.
     * @param p_event_id Event id the task receives Command events under; non-empty. A command whose branch is out of
     * range throws from the emitting call.
     * @param p_branch The branch recorded until a command changes the selection.
     * @throws std::runtime_error Under the conditions of the other constructor, if p_event_id is empty, or if no
     * EventManager exists.
     */
    RTMultiplexerVK(Vector<Unique<IRenderTaskVK>, RenderAllocator> p_branches, std::string_view p_event_id, uint32_t p_branch = 0);

    /**
     * @brief Combine branch task factories into one factory whose every task is an event-controlled RTMultiplexerVK.
     * The returned factory calls every branch factory with the graph context and multiplexes their tasks with the event-id
     * constructor. Every evaluation, for example after a resize, creates a new multiplexer on branch p_branch that
     * subscribes again.
     * @param p_branch_factories Factories of the branch tasks, indexed by branch; at least one, none empty.
     * @param p_event_id Event id every created multiplexer receives Command events under; non-empty.
     * @param p_render_data Render data whose allocator owns the multiplexers and their branch lists; it must outlive the
     * returned factory.
     * @param p_branch The branch every created multiplexer starts on; below the branch count.
     * @return The combined factory, suitable for RendTaskDescVK::factory.
     * @throws std::runtime_error If p_branch_factories is empty or holds an empty factory, p_branch is out of range,
     * p_event_id is empty, the render data or its allocator is null, or no EventManager exists.
     */
    static RendTaskFactoryVK combine_factories(Vector<RendTaskFactoryVK, RenderAllocator> p_branch_factories,
        std::string p_event_id,
        const RenderContextVK* p_render_data,
        uint32_t p_branch = 0);

    /**
     * @brief Select the branch recorded from the next graph recording on.
     * @throws std::runtime_error If p_branch is not below get_branch_count().
     */
    void set_branch(uint32_t p_branch);

    /**
     * @brief Get the selected branch.
     */
    uint32_t get_branch() const {
        return branch.load(std::memory_order_acquire);
    }

    /**
     * @brief Get the number of branches.
     */
    uint32_t get_branch_count() const {
        return static_cast<uint32_t>(branches.size());
    }

    virtual QueueFamilyVK queue_family() const override;
    /**
     * @brief Report the accesses of every branch, whichever is selected.
     */
    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override;
    /**
     * @brief Bind the resources of every branch.
     */
    virtual void bind_resources(const RendResViewVK& p_resources) override;
    /**
     * @brief Record the selected branch only.
     */
    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

private:
    Vector<Unique<IRenderTaskVK>, RenderAllocator> branches;
    std::atomic<uint32_t> branch;
    // Declared last, so the subscription ends before the members its callback uses are destroyed.
    EventRegistration command_registration;
};

} // namespace WhiteBirdEngine

#endif
