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
#ifndef WBE_FILE_VXGI_UTILS_SLANG
#define WBE_FILE_VXGI_UTILS_SLANG

struct BlockVoxelData {
    float4 color_occlusion;
    float3 average_vector;
};

struct VxgiNodeData {
    BlockVoxelData voxel_data[3][3][3];
};

#endif
