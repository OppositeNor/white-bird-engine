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
#include "utils.slang"

struct DescHeapProbeUniform {
    float4 value;
};

struct DescHeapProbeImages {
    [wbe::desc("sampled_image", "sampled_image")]
    uint2 sampled_image;
    [wbe::desc("storage_image", "storage_image")]
    uint2 storage_image;
};

struct DescHeapProbeIndices {
    [wbe::desc("uniform_buffer", "uniform_buffer")]
    uint2 uniform_buffer;
    DescHeapProbeImages images;
    [wbe::desc("storage_buffer", "storage_buffer", "read")]
    uint2 storage_buffer;
    [wbe::desc("sampler", "sampler")]
    uint2 sampler;
    [wbe::desc("uniform_buffer", "scalar_uniform")]
    uint2 scalar_uniform;
    [wbe::desc("acceleration_structure", "acceleration_structure")]
    uint2 acceleration_structure;
    [wbe::push_constant("time")]
    float time;
};

[vk::push_constant]
ConstantBuffer<DescHeapProbeIndices> di;

#ifdef WBE_REFLECTION_METADATA
RaytracingAccelerationStructure acceleration_structure;
#endif

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    ConstantBuffer<DescHeapProbeUniform> uniform_buffer =
        ResourceDescriptorHeap[di.uniform_buffer.x];
    ConstantBuffer<uint> scalar_uniform = ResourceDescriptorHeap[di.scalar_uniform.x];
    StructuredBuffer<float4> storage_buffer = ResourceDescriptorHeap[di.storage_buffer.x];
    Texture2D<float4> sampled_image = ResourceDescriptorHeap[di.images.sampled_image.x];
    RWTexture2D<float4> storage_image = ResourceDescriptorHeap[di.images.storage_image.x];
    SamplerState sampler = SamplerDescriptorHeap[di.sampler.x];
    storage_image[p_dispatch_thread_id.xy] =
        uniform_buffer.value + float(scalar_uniform) + storage_buffer[0] + sampled_image.SampleLevel(sampler, float2(0.0), 0.0);
}
