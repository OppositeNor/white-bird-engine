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

/**
 * @brief Vertex shader output / fragment shader input for the spot-light shadow map pass.
 */
struct ShadowVSOutput {
    /// Clip-space position of the vertex in light space.
    float4 position : SV_Position;
};

/**
 * @brief Constant buffer providing the spot light's view and projection matrices.
 */
struct ShadowDescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 light_camera_info;
    StaticSetDescriptorIndices static_set;
};

[[vk::push_constant]]
ConstantBuffer<ShadowDescriptorIndices> descriptor_indices;

/**
 * @brief Transform a static-set vertex into spot-light clip space.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_set.instances.
 * @return Clip-space vertex output.
 */
[shader("vertex")]
ShadowVSOutput vertex_main(Vertex3DTexturedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    ConstantBuffer<CameraInfo> light_camera_info = ResourceDescriptorHeap[descriptor_indices.light_camera_info.x];
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[descriptor_indices.static_set.instances.x];
    StructuredBuffer<NodeData> nodes = ResourceDescriptorHeap[descriptor_indices.static_set.nodes.x];
    InstanceData instance = instances[p_instance_id];
    float4 world_position = mul(nodes[instance.node_index].transform, float4(p_input.position.xyz, 1.0));
    ShadowVSOutput output;
    output.position = mul(light_camera_info.proj, mul(light_camera_info.view, world_position));
    return output;
}

/**
 * @brief Depth-only fragment stage. Writes a constant value into the dummy color attachment; the depth attachment is the real output.
 * @param p_input Interpolated vertex output.
 * @return Dummy color value (never read).
 */
[shader("fragment")]
float4 fragment_main(ShadowVSOutput p_input) : SV_Target0 {
    return float4(0.0, 0.0, 0.0, 1.0);
}
