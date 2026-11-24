/* Copyright 2026 OppositeNor

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
 * @file svogi_subvoxel_functions.slang
 * @brief Test module exposing the sub-voxel helper the SVOGI radiance injection uses to slangpy calls.
 * @details No defines are required.
 */
#include "svogi/svogi_utils.slang"

/**
 * @brief Runs svo_pos_to_subvoxel_pos() and returns the sub-voxel position in xyz and its index, as a float, in w.
 */
float4 svogi_subvoxel_position(float3 p_svo_position) {
    uint subvoxel_index;
    float3 subvoxel_position = svo_pos_to_subvoxel_pos(p_svo_position, subvoxel_index);
    return float4(subvoxel_position, float(subvoxel_index));
}
