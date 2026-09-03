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
#include "forward_cook_torrance_pbr/forward_cook_torrance_pbr_utils.slang"

struct RSMVSOutput {
    float4 position : SV_Position;
    float3 world_position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 tangent : TEXCOORD2;
    float3 bitangent : TEXCOORD3;
    float2 uv : TEXCOORD4;
    float material_index : TEXCOORD5;
};

ConstantBuffer<SpotLightInfo> light_camera_info;
StructuredBuffer<SubmeshData> static_submeshes;
StructuredBuffer<NodeData> static_nodes;
StructuredBuffer<InstanceData> static_instances;

/**
 * @brief Transform a static-set vertex for reflective shadow-map rendering.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_instances.
 * @return Reflective shadow-map vertex output.
 */
[shader("vertex")]
RSMVSOutput vertex_main(Vertex3DTexturedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    InstanceData instance = static_instances[p_instance_id];
    NodeData node = static_nodes[instance.node_index];
    float4 world_position = mul(node.transform, float4(p_input.position.xyz, 1.0));
    float3x3 direction_transform = (float3x3)node.transform;

    RSMVSOutput output;
    output.position = mul(light_camera_info.proj, mul(light_camera_info.view, world_position));
    output.world_position = world_position.xyz;
    output.normal = transform_static_normal(node.transform, p_input.normal);
    output.tangent = normalize(mul(direction_transform, p_input.tangent));
    output.bitangent = normalize(mul(direction_transform, p_input.bitangent));
    output.uv = p_input.uv;
    output.material_index = float(static_submeshes[instance.submesh_index].material_id);
    return output;
}
