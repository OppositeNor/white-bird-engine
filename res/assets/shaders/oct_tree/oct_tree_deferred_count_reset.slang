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
#include "utils/utils.slang"
#include "oct_tree/oct_tree_utils.slang"

struct OctTreeDeferredCountResetDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "oct_tree_info")]
    uint2 oct_tree_info;
};

[[vk::push_constant]]
ConstantBuffer<OctTreeDeferredCountResetDescriptorIndices> descriptor_indices;

/**
 * @brief Clears the deferred-list count selected by the shader variant.
 */
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    RWStructuredBuffer<OctTreeInfo> oct_tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
#ifdef WBE_RESET_PING_COUNT
    oct_tree_info[0].deferred_task_count_ping = 0;
#else
    oct_tree_info[0].deferred_task_count_pong = 0;
#endif
}
