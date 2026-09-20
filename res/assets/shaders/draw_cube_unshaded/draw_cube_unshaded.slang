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
#include "draw_cube_unshaded/draw_cube_unshaded_utils.slang"
#include "utils.slang"

#ifdef WBE_REFLECTION_METADATA
ParameterBlock<StaticSetContractMetadata> static_set;
#endif

/**
 * @brief The input data from the vertex buffer
 */
struct Vertex3DUnshadedInput {
    /**
     * @brief The position of the vertex.
     */
    [WBE_VERTEX_INPUT("position")]
    float3 position;
    /**
     * @brief The normal of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float3 normal;
    /**
     * @brief The uv of the vertex.
     */
    [WBE_VERTEX_INPUT]
    float2 uv;
};

/**
 * @brief Constant buffer providing the per-frame view and projection matrices.
 */
struct DrawCubeUnshadedDescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 camera_info;
    StaticSetDescriptorIndices static_set;
};

[[vk::push_constant]]
ConstantBuffer<DrawCubeUnshadedDescriptorIndices> descriptor_indices;

/**
 * @brief Transform an unshaded static-set vertex into clip space.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_set.instances.
 * @return Vertex output for the unshaded fragment stage.
 */
[shader("vertex")]
VSOutput vertex_main(Vertex3DUnshadedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    ConstantBuffer<CameraInfo> camera_info = ResourceDescriptorHeap[descriptor_indices.camera_info.x];
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[descriptor_indices.static_set.instances.x];
    StructuredBuffer<NodeData> nodes = ResourceDescriptorHeap[descriptor_indices.static_set.nodes.x];
    InstanceData instance = instances[p_instance_id];
    NodeData node = nodes[instance.node_index];
    float4 world_position = mul(node.transform, float4(p_input.position.xyz, 1.0));
    VSOutput output;
    float4 view_position =
        software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(camera_info.view), world_position);
    output.position = software_mul_float4x4_vector(WBE_SOFTWARE_MATRIX_ROWS(camera_info.proj), view_position);
    output.normal = transform_static_normal(node.normal_transform, p_input.normal);
    output.uv = p_input.uv;
    return output;
}

/**
 * @brief Visualize the interpolated surface normal as an RGB color, mapped from [-1, 1] to [0, 1].
 * @param p_input The interpolated vertex output.
 * @return The visualized normal as an opaque sRGB-ish color.
 */
[shader("fragment")]
float4 fragment_main(VSOutput p_input) : SV_Target0 {
    return float4(p_input.normal * 0.5 + 0.5, 1.0);
}
