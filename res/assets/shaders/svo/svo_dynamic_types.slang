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
#ifndef WBE_FILE_SVO_DYNAMIC_TYPES_SLANG
#define WBE_FILE_SVO_DYNAMIC_TYPES_SLANG

#include "svo/svo_common.slang"

/**
 * @brief Copy-on-write block of eight leaves; the parent's original description is saved in the parent node.
 */
struct SVODynamicLeafBlock {
    /**
     * @brief Copied static leaves or newly initialized dynamic leaves.
     */
    SVOLeaf leaves[8];
};

#endif
