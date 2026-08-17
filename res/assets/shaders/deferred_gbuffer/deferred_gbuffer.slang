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
#include "defs.slang"

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
 * @brief Transform a textured vertex into clip space and pass per-vertex attributes through to the fragment stage.
 * @param p_input The incoming vertex attributes.
 * @return The vertex output for the deferred G-buffer pass.
 */
[shader("vertex")]
VSOutput vertex_main(Vertex3DTexturedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    VSOutput output;
    float4 world_position = float4(p_input.position.xyz, 1.0);
    output.position       = mul(camera_info.proj, mul(camera_info.view, world_position));
    output.world_position = float4(p_input.position, float(p_instance_id));
    output.normal         = p_input.normal;
    output.tangent        = p_input.tangent;
    output.bitangent      = p_input.bitangent;
    output.uv             = p_input.uv;
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
    uint roughness_metallic_ao_index =
        uint(active_material.roughness_metallic_ao_index >= 0 ? active_material.roughness_metallic_ao_index : 0);
    uint albedo_descriptor_index = NonUniformResourceIndex(albedo_index);
    uint normal_descriptor_index = NonUniformResourceIndex(normal_index);
    uint roughness_metallic_ao_descriptor_index = NonUniformResourceIndex(roughness_metallic_ao_index);

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

    float3 rma = material_textures.textures[roughness_metallic_ao_descriptor_index]
                     .Sample(material_textures.material_sampler, p_input.uv)
                     .rgb;
    float flags = 0.0;
    output.rma_flags = float4(rma, flags);

    return output;
}
