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
#include "ddgi/ddgi_common.slang"

struct DDGIUpdateDescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 ddgi_config;
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 ddgi_time_info;
    [WBE_DESCRIPTOR("sampled_image")]
    uint2 probe_rays;
    [WBE_DESCRIPTOR("storage_image")]
    uint2 irradiance_atlas;
    [WBE_DESCRIPTOR("storage_image")]
    uint2 distance_atlas;
};

[[vk::push_constant]]
ConstantBuffer<DDGIUpdateDescriptorIndices> descriptor_indices;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    ConstantBuffer<DDGIConfig> config = ResourceDescriptorHeap[descriptor_indices.ddgi_config.x];
    ConstantBuffer<DDGITimeInfo> time_info = ResourceDescriptorHeap[descriptor_indices.ddgi_time_info.x];
    Texture2D<float4> probe_rays = ResourceDescriptorHeap[descriptor_indices.probe_rays.x];
    RWTexture2D<float4> irradiance_atlas = ResourceDescriptorHeap[descriptor_indices.irradiance_atlas.x];
    RWTexture2D<float2> distance_atlas = ResourceDescriptorHeap[descriptor_indices.distance_atlas.x];
    uint2 atlas_pixel = p_dispatch_thread_id.xy;
    uint probes_per_row = ddgi_grid_counts(config).x * ddgi_grid_counts(config).y;
    uint2 atlas_extent = uint2(probes_per_row, ddgi_grid_counts(config).z) * DDGI_DISTANCE_RESOLUTION;
    if (any(atlas_pixel >= atlas_extent)) {
        return;
    }
    uint2 tile = atlas_pixel / DDGI_DISTANCE_RESOLUTION;
    uint2 local_pixel = atlas_pixel % DDGI_DISTANCE_RESOLUTION;
    uint probe_index = tile.x + tile.y * probes_per_row;
    float2 octa_uv = (float2(local_pixel) + 0.5) / float(DDGI_DISTANCE_RESOLUTION);
    float3 direction = ddgi_octahedron_decode(octa_uv);
    float weighted_distance = 0.0;
    float weighted_distance_squared = 0.0;
    float distance_weight = 0.0;
    float3 irradiance = float3(0.0);
    bool write_irradiance = (local_pixel.x % 2U == 0U) && (local_pixel.y % 2U == 0U);
    float3 irradiance_direction = ddgi_octahedron_decode((float2(local_pixel / 2U) + 0.5) / float(DDGI_IRRADIANCE_RESOLUTION));
    for (uint ray_index = 0; ray_index < DDGI_RAYS_PER_PROBE; ++ray_index) {
        float3 ray_direction = ddgi_ray_direction(ray_index, time_info.ray_rotation_seed);
        float4 ray_result = probe_rays.Load(int3(ray_index, probe_index, 0));
        float distance_cosine = pow(max(dot(direction, ray_direction), 0.0), 8.0);
        weighted_distance += ray_result.a * distance_cosine;
        weighted_distance_squared += ray_result.a * ray_result.a * distance_cosine;
        distance_weight += distance_cosine;
        if (write_irradiance) {
            irradiance += ray_result.rgb * max(dot(irradiance_direction, ray_direction), 0.0);
        }
    }
    float inverse_weight = 1.0 / max(distance_weight, 0.0001);
    distance_atlas[atlas_pixel] = float2(weighted_distance, weighted_distance_squared) * inverse_weight;
    if (write_irradiance) {
        uint2 irradiance_pixel = tile * DDGI_IRRADIANCE_RESOLUTION + local_pixel / 2U;
        irradiance_atlas[irradiance_pixel] = float4(irradiance * (4.0 * WBE_PI / float(DDGI_RAYS_PER_PROBE)), 1.0);
    }
}
