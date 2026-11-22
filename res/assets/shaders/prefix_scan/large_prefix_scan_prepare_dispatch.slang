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
#include "prefix_scan/large_prefix_scan_utils.slang"

struct DescIndices {
    [wbe::desc("storage_buffer", "data_buffer", "read")]
    uint2 data_buffer;
    [wbe::desc("storage_buffer", "dispatch_buffer", "write")]
    uint2 dispatch_buffer;
    [wbe::push_constant("frame_slot")]
    uint shader_debug_frame_slot;
};

[vk::push_constant]
ConstantBuffer<DescIndices> di;

// Writes the indirect dispatch of the block passes of a large prefix scan, with one workgroup per block of the element
// count of the scanned buffer. Must be dispatched with exactly one workgroup.
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    CountedBuffer<WBE_PREFIX_SCAN_TYPE> data_buffer =
        CountedBuffer<WBE_PREFIX_SCAN_TYPE>(di.data_buffer, di.shader_debug_frame_slot);
    RWStructuredBuffer<DispatchIndirectCommand> dispatch_buffer = ResourceDescriptorHeap[di.dispatch_buffer.x];
    dispatch_buffer[0] = DispatchIndirectCommand(large_prefix_scan_block_num(data_buffer.size()));
}
