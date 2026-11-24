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
    [wbe::desc("storage_buffer", "keys", "read")] DescriptorHandle<RWByteAddressBuffer> keys;
    [wbe::desc("storage_buffer", "marks", "write")] DescriptorHandle<RWByteAddressBuffer> marks;
    [wbe::push_constant("frame_slot")] uint shader_debug_frame_slot;
};

[vk::push_constant]
ConstantBuffer<DescIndices> di;

// Marks the first key of every value in the sorted keys; see dedup_large_mark(). Must be dispatched with the per-key
// dispatch.
[shader("compute")]
[numthreads(WBE_DEDUP_LARGE_WORKGROUP_SIZE, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    CountedBuffer<uint> keys = CountedBuffer<uint>(di.keys, di.shader_debug_frame_slot);
    CountedBuffer<uint> marks = CountedBuffer<uint>(di.marks, di.shader_debug_frame_slot);
    dedup_large_mark(keys, marks, p_dispatch_thread_id.x);
}
