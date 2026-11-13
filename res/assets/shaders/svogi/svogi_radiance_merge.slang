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

struct DescriptorIndices {
    [wbe::descriptor("storage_buffer", "spot_light_infos", "read")]
    uint2 radiance_key_buffer;
    [wbe::descriptor("storage_buffer", "spot_light_infos", "read")]
    uint2 radiance_buffer;
    [wbe::descriptor("storage_buffer", "spot_light_infos", "write")]
    uint2 radiance_buffer_merged;
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> descriptor_indices;
