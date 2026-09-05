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
#include "utils.slang"

struct MaskedShadowVSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float material_index : TEXCOORD1;
};

ConstantBuffer<CameraInfo> light_camera_info;
ConstantBuffer<MaterialParamsBuffer> material_params;
ParameterBlock<MaterialTextures> material_textures;

/**
 * @brief Static geometry tables shared by both pipeline stages.
 */
ParameterBlock<StaticSet> static_set;

/**
 * @brief Transform a masked static-set vertex and resolve its material identifier.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_set.instances.
 * @return Clip-space vertex output with material metadata.
 */
[shader("vertex")]
MaskedShadowVSOutput vertex_main(Vertex3DTexturedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    InstanceData instance = static_set.instances[p_instance_id];
    float4 world_position = mul(static_set.nodes[instance.node_index].transform, float4(p_input.position.xyz, 1.0));
    MaskedShadowVSOutput output;
    output.position = mul(light_camera_info.proj, mul(light_camera_info.view, world_position));
    output.uv = p_input.uv;
    output.material_index = float(static_set.submeshes[instance.submesh_index].material_id);
    return output;
}

[shader("fragment")]
float4 fragment_main(MaskedShadowVSOutput p_input) : SV_Target0 {
    uint material_index = uint(round(p_input.material_index));
    MaterialParams active_material = material_params.materials[material_index];
    uint albedo_index = uint(active_material.albedo_index >= 0 ? active_material.albedo_index : 0);
    uint albedo_descriptor_index = NonUniformResourceIndex(albedo_index);
    float alpha =
        material_textures.textures[albedo_descriptor_index].Sample(material_textures.material_sampler, p_input.uv).a;
    if (alpha < 0.5) {
        discard;
    }
    return float4(0.0, 0.0, 0.0, 1.0);
}
