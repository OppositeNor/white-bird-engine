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

// Test-only compute shader enumerating macro variants: WBE_SHADER_VARIANT_SCALE selects a scale, and the optional
// WBE_SHADER_VARIANT_EXTRA adds an input buffer, so the variants differ in their reflected parameters.
struct ShaderVariantIndices {
    [wbe::desc("storage_buffer", "output_buffer", "write")]
    uint2 output_buffer;
#ifdef WBE_SHADER_VARIANT_EXTRA
    [wbe::desc("storage_buffer", "extra_buffer", "read")]
    uint2 extra_buffer;
#endif
};

[vk::push_constant]
ConstantBuffer<ShaderVariantIndices> shader_variant_indices;

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    RWStructuredBuffer<uint> output_buffer = ResourceDescriptorHeap[shader_variant_indices.output_buffer.x];
    uint value = p_dispatch_thread_id.x * WBE_SHADER_VARIANT_SCALE;
#ifdef WBE_SHADER_VARIANT_EXTRA
    StructuredBuffer<uint> extra_buffer = ResourceDescriptorHeap[shader_variant_indices.extra_buffer.x];
    value += extra_buffer[p_dispatch_thread_id.x];
#endif
    output_buffer[p_dispatch_thread_id.x] = value;
}
