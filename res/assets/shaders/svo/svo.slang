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
#ifndef WBE_FILE_SVO_SLANG
#define WBE_FILE_SVO_SLANG

#include "svo/svo_dynamic_types.slang"

/**
 * @brief Read-only view of an already constructed SVO that resolves positions to the data_index of their nodes and leaves.
 * @details Every stored node and leaf owns one data element through its data_index; the SVO does not own or read the data,
 * so callers index their own data buffer with the traced index. The construction passes assign the static node and leaf
 * data first, followed by the dynamic node data and then the dynamic leaf data. The SVO must be fully constructed and
 * published, and its buffers must not change during the calling pass, because node descriptions are read without
 * synchronization. Construction is not performed through this type.
 */
struct SVO {
    /**
     * @brief Static SVO node storage.
     */
    RWStructuredBuffer<SVONode> nodes;
    /**
     * @brief Dynamic SVO node storage; read only when traverse_dynamic is true.
     */
    RWStructuredBuffer<SVONode> dynamic_nodes;
    /**
     * @brief Static SVO leaf storage.
     */
    StructuredBuffer<SVOLeaf> leaves;
    /**
     * @brief Dynamic SVO leaf blocks; read only when traverse_dynamic is true.
     */
    StructuredBuffer<SVODynamicLeafBlock> dynamic_leaves;
    /**
     * @brief Spatial configuration of the SVO.
     */
    SVOInfo info;
    /**
     * @brief Whether traces follow the dynamic children published by the dynamic construction passes.
     */
    bool traverse_dynamic;

    /**
     * @brief Create a view over the buffers of a constructed SVO.
     */
    __init(RWStructuredBuffer<SVONode> p_nodes,
        RWStructuredBuffer<SVONode> p_dynamic_nodes,
        StructuredBuffer<SVOLeaf> p_leaves,
        StructuredBuffer<SVODynamicLeafBlock> p_dynamic_leaves,
        SVOInfo p_info,
        bool p_traverse_dynamic) {
        nodes = p_nodes;
        dynamic_nodes = p_dynamic_nodes;
        leaves = p_leaves;
        dynamic_leaves = p_dynamic_leaves;
        info = p_info;
        traverse_dynamic = p_traverse_dynamic;
    }

    /**
     * @brief Convert a world-space position into the SVO's unit cube.
     * @param p_world_position Position in world space.
     * @return The position relative to info.origin, scaled by 1 / info.width.
     */
    float3 get_svo_position(float3 p_world_position) {
        return (p_world_position - info.origin) / info.width;
    }

    /**
     * @brief Trace from the root to the deepest stored node or occupied leaf containing a position, and return its data index.
     * @details The trace stops at an occupied leaf, or at the deepest node whose child containing the position is empty or
     * an unoccupied leaf, so unoccupied regions return the data index of their coarsest stored ancestor. Dynamic children
     * are followed only when traverse_dynamic is true.
     * @param p_world_position Position in world space; every component must lie in [info.origin, info.origin + info.width).
     * @param p_trace_info Receives the final traversal state: the storage and index of the reached node or leaf, and its
     * depth.
     * @return The data_index of the reached node or leaf.
     */
    uint trace_data(float3 p_world_position, out SVONodeTraceInfo p_trace_info) {
        p_trace_info = get_svo_root_trace_info(get_svo_position(p_world_position), info.max_depth);
        traverse_svo(nodes, dynamic_nodes, info, traverse_dynamic, p_trace_info);
        return get_data_index(p_trace_info);
    }

    /**
     * @brief Get the data index of the node or leaf reached by a trace.
     * @param p_trace_info Traversal state produced by trace_data() or traverse_svo().
     * @return The data_index of the referenced node or leaf.
     */
    uint get_data_index(SVONodeTraceInfo p_trace_info) {
        switch (p_trace_info.index_type) {
        case SVONodeTraceInfo::IndexType::DYNAMIC_NODE:
            return dynamic_nodes[p_trace_info.node_index].data_index;
        case SVONodeTraceInfo::IndexType::STATIC_LEAF:
            return leaves[p_trace_info.node_index].data_index;
        case SVONodeTraceInfo::IndexType::DYNAMIC_LEAF:
            return dynamic_leaves[p_trace_info.node_index / 8].leaves[p_trace_info.node_index % 8].data_index;
        default:
            return nodes[p_trace_info.node_index].data_index;
        }
    }
};

#endif
