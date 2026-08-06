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
 * @brief Transform a textured vertex into clip space and pass per-vertex attributes through to the fragment stage.
 * @param p_input The incoming vertex attributes.
 * @return The vertex output for the deferred G-buffer pass.
 */
[shader("vertex")]
VSOutput vertex_main(Vertex3DTexturedInput p_input) {
    VSOutput output;
    float4 world_position = float4(p_input.position.xyz, 1.0);
    output.position       = mul(camera_info.proj, mul(camera_info.view, world_position));
    output.world_position = p_input.position;
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
 * @brief Pack interpolated surface attributes into the G-buffer outputs. Used by the renderer test as a stand-in for a real material shader.
 * @param p_input The interpolated vertex output.
 * @return The packed G-buffer attachments for this fragment.
 */
[shader("fragment")]
GBufferOutput fragment_main(VSOutput p_input) {
    GBufferOutput output;

    // Test material: simple colored albedo derived from uv, material id 1.
    float3 albedo = float3(p_input.uv.x, p_input.uv.y, 0.4);
    float material_id_unorm = 1.0 / 255.0; // material id 1 packed into 8-bit unorm.
    output.albedo_matid = float4(albedo, material_id_unorm);

    float3 normal = normalize(p_input.normal);
    output.octahedron_normal = octahedron_encode(normal);

    float roughness = 0.5;
    float metallic = 0.1;
    float ambient_occlusion = 1.0;
    float flags = 0.0;
    output.rma_flags = float4(roughness, metallic, ambient_occlusion, flags);

    return output;
}