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
/**
 * @file workgroup_prefix_sum.slang
 * @brief Test kernel that runs a workgroup prefix sum in place over one buffer on one workgroup.
 * @details Required defines: PREFIX_SUM, the prefix sum function to test; ELEMENT_COUNT; and KERNEL_WORKGROUP_SIZE.
 */
#include "utils/utils.slang"

RWStructuredBuffer<uint> data;
groupshared uint shared_data[ELEMENT_COUNT];

[shader("compute")]
[numthreads(KERNEL_WORKGROUP_SIZE, 1, 1)]
void main(uint3 p_group_thread_id : SV_GroupThreadID) {
    for (uint i = p_group_thread_id.x; i < ELEMENT_COUNT; i += KERNEL_WORKGROUP_SIZE) {
        shared_data[i] = data[i];
    }
    PREFIX_SUM(shared_data, p_group_thread_id.x, KERNEL_WORKGROUP_SIZE);
    for (uint i = p_group_thread_id.x; i < ELEMENT_COUNT; i += KERNEL_WORKGROUP_SIZE) {
        data[i] = shared_data[i];
    }
}
