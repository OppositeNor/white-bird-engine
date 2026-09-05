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
#include "deferred_gbuffer/deferred_gbuffer_utils.slang"

/**
 * @brief Constant buffer providing the per-frame view and projection matrices.
 */
ConstantBuffer<CameraInfo> camera_info;
/**
 * @brief Descriptor indices for all materials rendered by the indirect draw batch.
 */
ConstantBuffer<MaterialParamsBuffer> material_params;
/**
 * @brief Bindless material texture table and sampler.
 */
ParameterBlock<MaterialTextures> material_textures;

/**
 * @brief Static geometry tables shared by both pipeline stages.
 */
ParameterBlock<StaticSet> static_set;

/**
 * @brief Transform a static-set vertex for the deferred G-buffer pass.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_set.instances.
 * @return Vertex output in world and clip space.
 */
[shader("vertex")]
VSOutput vertex_main(Vertex3DTexturedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    InstanceData instance = static_set.instances[p_instance_id];
    NodeData node = static_set.nodes[instance.node_index];
    SubmeshData submesh = static_set.submeshes[instance.submesh_index];
    float4 world_position = mul(node.transform, float4(p_input.position.xyz, 1.0));
    float3x3 direction_transform = (float3x3)node.transform;

    VSOutput output;
    output.position = mul(camera_info.proj, mul(camera_info.view, world_position));
    output.world_position = float4(world_position.xyz, float(submesh.material_id));
    output.normal = transform_static_normal(node.transform, p_input.normal);
    output.tangent = normalize(mul(direction_transform, p_input.tangent));
    output.bitangent = normalize(mul(direction_transform, p_input.bitangent));
    output.uv = p_input.uv;
    return output;
}

/**
 * @brief Multi-render-target output written by the deferred G-buffer pass.
 */
struct GBufferOutput {
    /**
 * @brief Albedo (rgb) + material id (a). RGBA8 UNORM.
     */
    float4 albedo_matid : SV_Target0;
    /**
 * @brief Octahedron-encoded surface normal stored into R16G16_SNORM ([-1, 1]).
     */
    float2 octahedron_normal : SV_Target1;
    /**
 * @brief Roughness (r), metallic (g), ambient occlusion (b), flags (a). RGBA8 UNORM.
     */
    float4 rma_flags : SV_Target2;
};

/**
 * @brief Sample the active material and pack its surface data into the G-buffer outputs.
 * @param p_input The interpolated vertex output.
 * @return The packed G-buffer attachments for this fragment.
 */
[shader("fragment")]
GBufferOutput fragment_main(VSOutput p_input) {
    GBufferOutput output;

    uint material_index = uint(round(p_input.world_position.w));
    MaterialParams active_material = material_params.materials[material_index];
    uint albedo_index = uint(active_material.albedo_index >= 0 ? active_material.albedo_index : 0);
    uint normal_index = uint(active_material.normal_index >= 0 ? active_material.normal_index : 0);
    uint rma_index = uint(active_material.rma_index >= 0 ? active_material.rma_index : 0);
    uint albedo_descriptor_index = NonUniformResourceIndex(albedo_index);
    uint normal_descriptor_index = NonUniformResourceIndex(normal_index);
    uint rma_descriptor_index = NonUniformResourceIndex(rma_index);

    float4 albedo_sample =
        material_textures.textures[albedo_descriptor_index].Sample(material_textures.material_sampler, p_input.uv);
    if (albedo_sample.a < 0.5) {
        discard;
    }
    float material_id_unorm = float(material_index) / 255.0;
    output.albedo_matid = float4(albedo_sample.rgb, material_id_unorm);

    float3 sampled_normal =
        material_textures.textures[normal_descriptor_index].Sample(material_textures.material_sampler, p_input.uv).rgb;
    float3 normal = normal_from_texture(sampled_normal, p_input.normal, p_input.tangent, p_input.bitangent);
    output.octahedron_normal = octahedron_encode(normal);

    float3 rma = material_textures.textures[rma_descriptor_index]
                     .Sample(material_textures.material_sampler, p_input.uv)
                     .rgb;
    float flags = 0.0;
    output.rma_flags = float4(rma, flags);

    return output;
}
