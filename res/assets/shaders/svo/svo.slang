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
 * @brief Read-only view of an already constructed SVO whose nodes and leaves index a buffer of DataType.
 * @details Every stored node and leaf owns one element of the data buffer through its data_index. The data buffer holds
 * the static node and leaf data first, followed by the dynamic node data and then the dynamic leaf data, as assigned by
 * the construction passes. The SVO must be fully constructed and published, and its buffers must not change during the
 * calling pass, because node descriptions are read without synchronization. Construction is not performed through this
 * type.
 * @tparam DataType Element type of the data buffer.
 */
struct SVO<DataType> {
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
     * @brief Data indexed by the data_index of every node and leaf.
     */
    StructuredBuffer<DataType> data;
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
        StructuredBuffer<DataType> p_data,
        SVOInfo p_info,
        bool p_traverse_dynamic) {
        nodes = p_nodes;
        dynamic_nodes = p_dynamic_nodes;
        leaves = p_leaves;
        dynamic_leaves = p_dynamic_leaves;
        data = p_data;
        info = p_info;
        traverse_dynamic = p_traverse_dynamic;
    }

    /**
     * @brief Trace from the root to the deepest stored node or occupied leaf containing a position, and return its data.
     * @details The trace stops at an occupied leaf, or at the deepest node whose child containing the position is empty or
     * an unoccupied leaf, so unoccupied regions return the data of their coarsest stored ancestor. Dynamic children are
     * followed only when traverse_dynamic is true.
     * @param p_svo_position Position in the SVO's unit cube; every component must lie in [0, 1).
     * @param p_trace_info Receives the final traversal state: the storage and index of the reached node or leaf, and its
     * depth.
     * @return The data of the reached node or leaf.
     */
    DataType trace_data(float3 p_svo_position, out SVONodeTraceInfo p_trace_info) {
        p_trace_info = get_svo_root_trace_info(p_svo_position, info.max_depth);
        traverse_svo(nodes, dynamic_nodes, info, traverse_dynamic, p_trace_info);
        return data[get_data_index(p_trace_info)];
    }

    /**
     * @brief Get the data-buffer index of the node or leaf reached by a trace.
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
