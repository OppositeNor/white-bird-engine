#include "radix_sort/sort.slang"

struct BufferInfo {
    uint buffer_size;
};

struct DescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer", "buffer")] uint2 buffer;
    [WBE_DESCRIPTOR("uniform_buffer", "buffer_info")] uint2 buffer_info;
};

[vk::push_constant]
ConstantBuffer<DescriptorIndices> decriptor_indices;

[numthreads(64, 1, 1)]
void main(uint3 p_group_id : SV_GroupID, uint3 p_group_thread_id : SV_GroupThreadID) {
    RWStructuredBuffer<uint> buffer = decriptor_indices.buffer.x;
    ConstantBuffer<BufferInfo> buffer_info = decriptor_indices.buffer_info.x;
    // TODO
}
