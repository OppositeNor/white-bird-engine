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
#ifndef WBE_FILE_I_RENDER_TASK_VK_HH
#define WBE_FILE_I_RENDER_TASK_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "function/render/i_render_task.hh"
#include "function/render/render_defs.hh"
#include <cstdint>
#include <string>
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
 * @brief A Vulkan resource binding (forward-declared here so that IRenderTaskVK
 * subclasses can refer to it without including the render graph header).
 */
struct RendResVK;
class RendResViewVK;

/**
 * @class RendResUsageVK
 * @brief Task-facing resource usage categories translated privately by the render graph into Vulkan synchronization scopes.
 */
enum class RendResUsageVK : uint32_t {
    /** Shader-sampled image read. */
    SAMPLED_IMAGE,
    /** Color attachment read or write. */
    COLOR_ATTACHMENT,
    /** Depth/stencil attachment read or write. */
    DEPTH_ATTACHMENT,
    /** Shader storage image read or write. */
    STORAGE_IMAGE,
    /** Uniform buffer read. */
    UNIFORM_BUFFER,
    /** Shader storage buffer read or write. */
    STORAGE_BUFFER,
    /** Acceleration-structure build write or shader read. */
    ACCELERATION_STRUCTURE,
    /** Transfer source read. */
    TRANSFER_SRC,
    /** Transfer destination write. */
    TRANSFER_DST,
    /** Indirect command buffer read. */
    INDIRECT_BUFFER,
    /** Vertex buffer read. */
    VERTEX_BUFFER,
    /** Index buffer read. */
    INDEX_BUFFER,
};

/**
 * @class ResAccessVK
 * @brief Compact resource declaration emitted by a Vulkan render task or graph descriptor.
 */
struct ResAccessVK {
    /** Resource identifier used by the graph. */
    std::string res_id;
    /** Task-facing usage translated internally to Vulkan access and stage masks. */
    RendResUsageVK usage = RendResUsageVK::SAMPLED_IMAGE;
};

/**
 * @class IRenderTaskVK
 * @brief Vulkan render task. A IRenderTaskVK records work into a single
 * Vulkan command buffer per frame; the owning RenderGraphVK schedules it
 * onto a queue family chosen by `queue_family()` and synchronises it with
 * other tasks via the read/write resource declarations and execution edges.
 */
struct IRenderTaskVK : public IRenderTask {
    IRenderTaskVK() = default;
    virtual ~IRenderTaskVK() = default;
    IRenderTaskVK(const IRenderTaskVK&) = default;
    IRenderTaskVK(IRenderTaskVK&&) noexcept = default;
    IRenderTaskVK& operator=(const IRenderTaskVK&) = default;
    IRenderTaskVK& operator=(IRenderTaskVK&&) noexcept = default;

    /**
     * @brief The queue family this task should run on. The render graph uses
     * this to select the command pool/queue and to decide whether resource
     * edges need queue ownership transfer barriers.
     *
     * @return The queue family index.
     */
    virtual QueueFamilyVK queue_family() const = 0;

    /**
     * @brief Bind stable graph resource containers before the task is first recorded.
     *
     * Tasks retain the containers they use and resolve the current render object from each container during record().
     * Resource ids remain graph-side synchronization identities and are not looked up while recording.
     *
     * @param p_resources Resources indexed by the graph descriptor during graph construction.
     */
    virtual void bind_resources(const RendResViewVK& p_resources) {
        static_cast<void>(p_resources);
    }

    /**
     * @brief The resources read intrinsically by this task.
     *
     * @return The resource access declarations for reads performed by this task.
     */
    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const = 0;

    /**
     * @brief The resources written intrinsically by this task.
     *
     * @return The resource access declarations for writes performed by this task.
     */
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const = 0;

    /**
     * @brief Record this task's commands into the given command buffer. The
     * command buffer has already been begun by the graph and will be ended by
     * the graph after this call returns. Timeline-semaphore waits required by
     * incoming resource dependencies accompany this task's submission;
     * required image layout transitions are emitted first.
     *
     * @param p_command_buffer The command buffer to record into.
     * @param p_delta     The frame delta in seconds.
     */
    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) = 0;

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
