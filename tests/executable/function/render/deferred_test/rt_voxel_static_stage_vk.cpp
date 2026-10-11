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
#include "rt_voxel_static_stage_vk.hh"
#include "core/logging/log.hh"
#include "utils/defs.hh"
#include <utility>

namespace WhiteBirdEngine {

RTVoxelStaticStageVK::RTVoxelStaticStageVK(
    Unique<IRenderTaskVK> p_task, Ref<bool> p_completed, bool p_finish, std::string_view p_completion_message)
    : task(std::move(p_task)), completed(std::move(p_completed)), finish(p_finish), completion_message(p_completion_message) {
    WBE_DEBUG_ASSERT(task != MEM_NULL && completed != MEM_NULL);
}

void RTVoxelStaticStageVK::record(VkCommandBuffer p_buffer, double p_delta) {
    if (*completed) {
        return;
    }
    task->record(p_buffer, p_delta);
    if (finish) {
        *completed = true;
        stdout_log(WBE_CHANNEL_DEBUG)->message(completion_message);
    }
}

} // namespace WhiteBirdEngine
