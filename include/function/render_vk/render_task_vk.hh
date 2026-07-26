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
#ifndef WBE_FILE_RENDER_TASK_VK_HH
#define WBE_FILE_RENDER_TASK_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_task.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class QueueFamilyVK
 * @brief Logical queue families a Vulkan render task can request. The
 * RenderGraphVK maps these to the concrete VkQueue / VkCommandPool from
 * BaseRenderDataVK.
 */
enum class QueueFamilyVK : uint32_t {
    GRAPHICS = 0,
    COMPUTE = 1,
};

/**
 * @class RendResVK
 * @brief A Vulkan resource binding (forward-declared here so that RenderTaskVK
 * subclasses can refer to it without including the render graph header).
 */
struct RendResVK;

/**
 * @class ResAccessVK
 * @brief Per-resource access metadata declared by a Vulkan render task. The
 * render graph uses this to compute pipeline barriers between consumer and
 * producer nodes that share the same resource.
 */
struct ResAccessVK {
    // The id of the resource consumed/produced by the task.
    RendResID res_id = 0;
    // Combined access flags for this access (read or write side as
    // appropriate).
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    // Pipeline stage(s) at which the access occurs.
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
};

/**
 * @class RenderTaskVK
 * @brief Vulkan render task. A RenderTaskVK records work into a single
 * Vulkan command buffer per frame; the owning RenderGraphVK schedules it
 * onto a queue family chosen by `queue_family()` and synchronises it with
 * other tasks via the read/write resource declarations and execution edges.
 */
struct RenderTaskVK : public RenderTask {
    RenderTaskVK() = default;
    virtual ~RenderTaskVK() = default;
    RenderTaskVK(const RenderTaskVK&) = default;
    RenderTaskVK(RenderTaskVK&&) noexcept = default;
    RenderTaskVK& operator=(const RenderTaskVK&) = default;
    RenderTaskVK& operator=(RenderTaskVK&&) noexcept = default;

    /**
     * @brief The queue family this task should run on. The render graph uses
     * this to select the command pool/queue and to decide whether resource
     * edges need queue ownership transfer barriers.
     *
     * @return The queue family index.
     */
    virtual QueueFamilyVK queue_family() const = 0;

    /**
     * @brief The list of resources read by this task, with the access type and
     * stage at which they are read. Used by the graph to build pipeline
     * barriers from the producing task to this one.
     */
    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const = 0;

    /**
     * @brief The list of resources written by this task, with the access type
     * and stage at which they are written. Used by the graph to build pipeline
     * barriers from this task to its consumer(s).
     */
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const = 0;

    /**
     * @brief Record this task's commands into the given command buffer. The
     * command buffer has already been begun by the graph and will be ended by
     * the graph after this call returns. Pipeline barriers required by the
     * resource edges into this task have already been emitted before this is
     * invoked.
     *
     * @param p_command_buffer The command buffer to record into.
     * @param p_resources All bound resources for the active pipeline, indexed
     *                    by resource id.
     * @param p_delta     The frame delta in seconds.
     */
    virtual void record(VkCommandBuffer p_command_buffer, const Vector<RendResVK, RenderAllocator>& p_resources, double p_delta) = 0;

    /**
     * @brief Default no-op for the cross-API perform() entry point. The
     * Vulkan render graph drives execution via record(); perform() is kept
     * only to satisfy the abstract interface.
     */
    virtual void perform(double p_delta) override {
        (void)p_delta;
    }

    /**
     * @brief Default no-op init.
     */
    virtual void init() override {
    }

    /**
     * @class RecordCommandBufferGuard
     * @brief RAII guard for recording a Vulkan command buffer. Begins on
     * construction and ends on destruction.
     */
    class RecordCommandBufferGuard {
    public:
        RecordCommandBufferGuard() = delete;
        ~RecordCommandBufferGuard();
        RecordCommandBufferGuard(const RecordCommandBufferGuard&) = delete;
        RecordCommandBufferGuard(RecordCommandBufferGuard&&) noexcept = delete;
        RecordCommandBufferGuard& operator=(const RecordCommandBufferGuard&) = delete;
        RecordCommandBufferGuard& operator=(RecordCommandBufferGuard&&) noexcept = delete;

        /**
         * @brief Constructor.
         *
         * @param p_command_buffer   The command buffer to record into.
         * @param p_inheritance_info The inheritance info, used for secondary
         *                           command buffers; nullptr for primary.
         */
        RecordCommandBufferGuard(VkCommandBuffer p_command_buffer, VkCommandBufferInheritanceInfo* p_inheritance_info = nullptr);

        VkCommandBuffer command_buffer;
    };
};

} // namespace WhiteBirdEngine

#endif
