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
#ifndef WBE_FILE_RT_VOXEL_FRAME_BEGIN_VK_HH
#define WBE_FILE_RT_VOXEL_FRAME_BEGIN_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class RTVoxelFrameBeginVK
 * @brief Orders mutation of a shared voxel structure after the preceding frame's graphics consumers.
 * @details Records a full memory barrier on the graphics queue and reports an all-commands write of one resource, which the
 * first mutating task of the frame must be connected to. Neither copyable nor movable; confined to the rendering thread.
 */
class RTVoxelFrameBeginVK final : public IRenderTaskVK {
public:
    RTVoxelFrameBeginVK() = delete;
    /**
     * @brief Creates the frame-begin barrier.
     * @param p_resource_id Graph resource carrying the edge to the first mutating task.
     */
    explicit RTVoxelFrameBeginVK(std::string_view p_resource_id);
    virtual ~RTVoxelFrameBeginVK() override = default;
    RTVoxelFrameBeginVK(const RTVoxelFrameBeginVK&) = delete;
    RTVoxelFrameBeginVK(RTVoxelFrameBeginVK&&) = delete;
    RTVoxelFrameBeginVK& operator=(const RTVoxelFrameBeginVK&) = delete;
    RTVoxelFrameBeginVK& operator=(RTVoxelFrameBeginVK&&) = delete;

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::GRAPHICS;
    }
    virtual void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const override;
    virtual void record(VkCommandBuffer p_buffer, double p_delta) override;

private:
    std::string resource_id;
};

} // namespace WhiteBirdEngine

#endif
