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
#include "rt_voxel_frame_begin_vk.hh"
#include <stdexcept>
#include <string>

namespace WhiteBirdEngine {

RTVoxelFrameBeginVK::RTVoxelFrameBeginVK(std::string_view p_resource_id) : resource_id(p_resource_id) {
    if (resource_id.empty()) {
        throw std::runtime_error("Failed to create the voxel frame-begin task: resource id is empty.");
    }
}

void RTVoxelFrameBeginVK::append_accesses(
    Vector<ResAccessVK, RenderAllocator>& /*p_reads*/, Vector<ResAccessVK, RenderAllocator>& p_writes) const {
    // The full memory barrier below covers every command, which the edge to the first mutating task synchronizes after.
    p_writes.push_back({.res_id = resource_id, .usage = RendResUsageVK::ALL_COMMANDS});
}

void RTVoxelFrameBeginVK::record(VkCommandBuffer p_buffer, double /*p_delta*/) {
    VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
    };
    VkDependencyInfo dependency{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(p_buffer, &dependency);
}

} // namespace WhiteBirdEngine
