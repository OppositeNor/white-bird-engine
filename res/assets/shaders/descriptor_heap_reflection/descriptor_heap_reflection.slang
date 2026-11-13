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

struct DescriptorHeapProbeUniform {
    float4 value;
};

struct DescriptorHeapProbeImages {
    [wbe::descriptor("sampled_image", "sampled_image")]
    uint2 sampled_image;
    [wbe::descriptor("storage_image", "storage_image")]
    uint2 storage_image;
};

struct DescriptorHeapProbeIndices {
    [wbe::descriptor("uniform_buffer", "uniform_buffer")]
    uint2 uniform_buffer;
    DescriptorHeapProbeImages images;
    [wbe::descriptor("storage_buffer", "storage_buffer", "read")]
    uint2 storage_buffer;
    [wbe::descriptor("sampler", "sampler")]
    uint2 sampler;
    [wbe::descriptor("uniform_buffer", "scalar_uniform")]
    uint2 scalar_uniform;
    [wbe::descriptor("acceleration_structure", "acceleration_structure")]
    uint2 acceleration_structure;
    [wbe::push_constant("time")]
    float time;
};

[vk::push_constant]
ConstantBuffer<DescriptorHeapProbeIndices> descriptor_indices;

#ifdef WBE_REFLECTION_METADATA
RaytracingAccelerationStructure acceleration_structure;
#endif

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    ConstantBuffer<DescriptorHeapProbeUniform> uniform_buffer =
        ResourceDescriptorHeap[descriptor_indices.uniform_buffer.x];
    ConstantBuffer<uint> scalar_uniform = ResourceDescriptorHeap[descriptor_indices.scalar_uniform.x];
    StructuredBuffer<float4> storage_buffer = ResourceDescriptorHeap[descriptor_indices.storage_buffer.x];
    Texture2D<float4> sampled_image = ResourceDescriptorHeap[descriptor_indices.images.sampled_image.x];
    RWTexture2D<float4> storage_image = ResourceDescriptorHeap[descriptor_indices.images.storage_image.x];
    SamplerState sampler = SamplerDescriptorHeap[descriptor_indices.sampler.x];
    storage_image[p_dispatch_thread_id.xy] =
        uniform_buffer.value + float(scalar_uniform) + storage_buffer[0] + sampled_image.SampleLevel(sampler, float2(0.0), 0.0);
}
