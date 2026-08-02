// fragment
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

/**
 * @brief Vertex shader output / fragment shader input for the deferred albedo debug fullscreen pass.
 */
struct VSOutput {
    /**
 * @brief Clip-space position of the fullscreen triangle vertex.
     */
    float4 position : SV_Position;
    /**
 * @brief Screen-space uv used to sample the G-buffer albedo target.
     */
    float2 uv       : TEXCOORD0;
};

/**
 * @brief Combined image+sampler for the G-buffer albedo attachment.
 */
Sampler2D albedo_sampler;

/**
 * @brief Output the sampled G-buffer albedo color as an opaque pixel.
 * @param p_input The interpolated vertex output from the fullscreen triangle.
 * @return The albedo color with alpha forced to 1.0.
 */
float4 main(VSOutput p_input) : SV_Target0 {
    float3 albedo = albedo_sampler.Sample(p_input.uv).rgb;
    return float4(albedo, 1.0);
}
