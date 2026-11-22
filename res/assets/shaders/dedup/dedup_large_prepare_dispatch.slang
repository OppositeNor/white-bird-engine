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
#include "dedup/dedup_large_utils.slang"

struct DescIndices {
    [wbe::desc("storage_buffer", "keys", "read_write")] uint2 keys;
    [wbe::desc("storage_buffer", "dispatch_buffer", "write")] uint2 dispatch_buffer;
    [wbe::push_constant("frame_slot")] uint shader_debug_frame_slot;
};

[vk::push_constant]
ConstantBuffer<DescIndices> di;

// Prepares the keys for the large deduplication: writes the number of local sorts of the radix sort to the metadata of
// the keys, and the indirect dispatch of the per-key passes. Must be dispatched with exactly one workgroup.
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    uint frame_slot = di.shader_debug_frame_slot;
    CountedBuffer<uint> keys = CountedBuffer<uint>(di.keys, frame_slot);
    RWStructuredBuffer<DispatchIndirectCommand> dispatch_buffer = ResourceDescriptorHeap[di.dispatch_buffer.x];
    uint key_count = keys.size();
    keys.set_header(WBE_RADIX_SORT_LOCAL_SORT_NUM_INDEX, radix_sort_local_sort_num(key_count));
    dispatch_buffer[0] = DispatchIndirectCommand(dedup_large_group_num(key_count));
}
