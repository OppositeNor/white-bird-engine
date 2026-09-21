#ifndef WBE_OCT_TREE_BUILD_SLANG
#define WBE_OCT_TREE_BUILD_SLANG

#include "utils/utils.slang"
#include "oct_tree/oct_tree_utils.slang"

/**
 * @brief The input data from the vertex buffer
 */
struct VSInput {
    /**
     * @brief The position of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 position;
};

/**
 * @brief Vertex shader output / fragment shader input for the deferred G-buffer pass.
 */
struct VSOutput {
    /**
     * @brief Clip-space position.
     */
    float4 position       : SV_Position;
    /**
     * @brief Position in SVO space (0, 0 is the corner of the oct tree's region, and values inside the
     * oct tree are ranged [0, 1)^3)
     */
    float3 svo_position : TEXCOORD0;
};


struct OctTreeDescriptorIndices {
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 oct_tree_info;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 tree_nodes;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 tree_leaves;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 tree_node_data;
    [WBE_DESCRIPTOR("storage_buffer")]
    uint2 node_data;

    StaticSetDescriptorIndices static_set;
};

[[vk::push_constant]]
ConstantBuffer<OctTreeDescriptorIndices> descriptor_indices;

/**
 * @brief Transform a static-set vertex for the oct tree.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_set.instances.
 * @return Vertex output in clip space.
 */
[shader("vertex")]
VSOutput vertex_main(VSInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    ConstantBuffer<OctTreeInfo> oct_tree_info = ResourceDescriptorHeap[descriptor_indices.oct_tree_info.x];
    float4x4 proj_matrix = oct_tree_info.matrices[WBE_OCT_TREE_VIEW_INDEX];
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[descriptor_indices.static_set.instances.x];
    StructuredBuffer<NodeData> nodes = ResourceDescriptorHeap[descriptor_indices.static_set.nodes.x];
    StructuredBuffer<SubmeshData> submeshes = ResourceDescriptorHeap[descriptor_indices.static_set.submeshes.x];

    InstanceData instance = instances[p_instance_id];
    NodeData node = nodes[instance.node_index];
    SubmeshData submesh = submeshes[instance.submesh_index];
    float4 world_position = mul(node.transform, float4(p_input.position.xyz, 1.0));
    float3x3 direction_transform = (float3x3)node.transform;

    VSOutput output;
    output.position = software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(proj_matrix), world_position);
    output.svo_position = (world_position.xyz - oct_tree_info.origin) / oct_tree_info.width;
    return output;
}

[shader("fragment")]
void fragment_main(VSOutput p_input) : SV_Target0 {
    // Trace to the bottom-most node

}

#endif
