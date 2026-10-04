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
#include "prefix_sum/prefix_sum_utils.slang"

struct DescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "buffer", "write")] uint2 buffer;
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> descriptor_indices;

[numthreads(64, 1, 1)]
void main(uint3 p_group_thread_id : SV_GroupThreadID) {
    RWStructuredBuffer<uint> buffer = ResourceDescriptorHeap[descriptor_indices.buffer];
    uint bucket_num = buffer[0];
    uint endian_start = buffer[1];
    workgroup_general_prefix_sum<64>(buffer, endian_start, bucket_num, p_group_thread_id.x);
}

