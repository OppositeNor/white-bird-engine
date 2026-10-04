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
 * @file octree_traverse.slang
 * @brief Test kernel that runs one octree traversal per entry of traces and writes the final state back in place.
 * @details Required defines: TRAVERSE_STATIC, nonzero to call traverse_static_octree() and zero to call
 * traverse_octree(); and TRAVERSE_DYNAMIC, the p_traverse_dynamic argument of traverse_octree().
 */
#include "utils/utils.slang"
#include "octree/octree_common.slang"

RWStructuredBuffer<OctreeNode> nodes;
RWStructuredBuffer<OctreeNode> dynamic_nodes;
RWStructuredBuffer<OctreeInfo> tree_info;
RWStructuredBuffer<OctreeNodeTraceInfo> traces;

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    uint trace_count;
    uint trace_stride;
    traces.GetDimensions(trace_count, trace_stride);
    uint index = p_dispatch_thread_id.x;
    if (index >= trace_count) {
        return;
    }
    OctreeNodeTraceInfo trace_info = traces[index];
#if TRAVERSE_STATIC
    traverse_static_octree(nodes, trace_info);
#else
    traverse_octree(nodes, dynamic_nodes, tree_info[0], TRAVERSE_DYNAMIC != 0, trace_info);
#endif
    traces[index] = trace_info;
}
