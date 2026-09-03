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

struct Vertex3DUnshadedInput {
    [WBE_VERTEX_INPUT("position")]
    float3 position;
    [WBE_VERTEX_INPUT]
    float3 normal;
    [WBE_VERTEX_INPUT]
    float2 uv;
};

ConstantBuffer<CameraInfo> camera_info;
StructuredBuffer<NodeData> static_nodes;
StructuredBuffer<InstanceData> static_instances;

/**
 * @brief Transform an unshaded static-set vertex into clip space.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_instances.
 * @return Vertex output for the unshaded fragment stage.
 */
[shader("vertex")]
VSOutput vertex_main(Vertex3DUnshadedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    InstanceData instance = static_instances[p_instance_id];
    NodeData node = static_nodes[instance.node_index];
    float3x3 direction_transform = (float3x3)node.transform;
    float4 world_position = mul(node.transform, float4(p_input.position.xyz, 1.0));
    VSOutput output;
    output.position = mul(camera_info.proj, mul(camera_info.view, world_position));
    output.normal = normalize(mul(direction_transform, p_input.normal));
    output.uv = p_input.uv;
    return output;
}
