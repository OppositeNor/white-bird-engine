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
 * @file octree_common_functions.slang
 * @brief Test module exposing the octree common, deferred-list, and dynamic-type helpers to slangpy calls.
 * @details octree/octree_common.slang relies on utils/utils.slang without including it, so this module includes both.
 * Helpers taking an OctreeNode are wrapped to take its packed children_data. No defines are required.
 */
#include "utils/utils.slang"
#include "octree/octree_dynamic_types.slang"

/**
 * @brief Decodes the ChildrenState of a node with the given packed description.
 */
uint octree_children_state(uint p_children_data) {
    OctreeNode node;
    node.children_data = p_children_data;
    return uint(get_octree_children_state(node));
}

/**
 * @brief Decodes the first child index of a node with the given packed description.
 */
uint octree_children_index(uint p_children_data) {
    OctreeNode node;
    node.children_data = p_children_data;
    return get_octree_children_index(node);
}

/**
 * @brief Returns (OCTREE_CHILDREN_OCCUPANCY_MASK, OCTREE_CHILDREN_BLOCK_INDEX_MASK, OCTREE_CHILDREN_PAYLOAD_MASK).
 */
uint3 octree_children_masks() {
    return uint3(OCTREE_CHILDREN_OCCUPANCY_MASK, OCTREE_CHILDREN_BLOCK_INDEX_MASK, OCTREE_CHILDREN_PAYLOAD_MASK);
}

/**
 * @brief Returns (OCTREE_CHILDREN_STATE_SHIFT, OCTREE_CHILDREN_OCCUPANCY_SHIFT, OCTREE_DEFERRED_LIST_COUNT_INDEX,
 * OCTREE_DEFERRED_LIST_ENTRY_OFFSET).
 */
uint4 octree_layout_constants() {
    return uint4(OCTREE_CHILDREN_STATE_SHIFT, OCTREE_CHILDREN_OCCUPANCY_SHIFT, OCTREE_DEFERRED_LIST_COUNT_INDEX,
        OCTREE_DEFERRED_LIST_ENTRY_OFFSET);
}
