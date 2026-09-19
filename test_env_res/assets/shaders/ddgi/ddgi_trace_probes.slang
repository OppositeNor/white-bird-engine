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
#include "deferred_composition/deferred_composition_utils.slang"
#include "ddgi/ddgi_common.slang"

struct DDGITraceDescriptorIndices {
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 ddgi_config;
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 ddgi_time_info;
    [WBE_DESCRIPTOR("uniform_buffer")]
    uint2 spot_light_info;
    [WBE_DESCRIPTOR("acceleration_structure")]
    uint2 static_opaque_tlas;
    [WBE_DESCRIPTOR("storage_image")]
    uint2 probe_rays;
};

[[vk::push_constant]]
ConstantBuffer<DDGITraceDescriptorIndices> descriptor_indices;

#ifdef WBE_REFLECTION_METADATA
RaytracingAccelerationStructure static_opaque_tlas;
#endif

float3 ddgi_sky_radiance(float3 p_direction) {
    return lerp(float3(0.10, 0.09, 0.08), float3(0.20, 0.25, 0.33), saturate(p_direction.y * 0.5 + 0.5));
}

float3 ddgi_shade_hit(RaytracingAccelerationStructure p_acceleration_structure,
    SpotLightInfo p_light,
    float3 p_position,
    float3 p_normal) {
    float3 to_light = p_light.position - p_position;
    float distance_to_light = length(to_light);
    float3 light_direction = to_light / max(distance_to_light, 0.0001);
    float normal_factor = max(dot(p_normal, light_direction), 0.0);
    float cone_cosine = dot(-light_direction, normalize(p_light.direction));
    float cone_factor = saturate((cone_cosine - p_light.outer_cone_cos) /
        max(p_light.inner_cone_cos - p_light.outer_cone_cos, 0.0001));
    float range_factor = saturate(1.0 - distance_to_light / max(p_light.range, 0.0001));
    if (normal_factor <= 0.0 || cone_factor <= 0.0 || range_factor <= 0.0) {
        return float3(0.0);
    }
    RayDesc shadow_ray;
    shadow_ray.Origin = p_position + p_normal * 0.02;
    shadow_ray.Direction = light_direction;
    shadow_ray.TMin = 0.02;
    shadow_ray.TMax = max(distance_to_light - 0.04, 0.02);
    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadow_query;
    shadow_query.TraceRayInline(p_acceleration_structure, RAY_FLAG_NONE, 0xFF, shadow_ray);
    while (shadow_query.Proceed()) {
    }
    if (shadow_query.CommittedStatus() != COMMITTED_NOTHING) {
        return float3(0.0);
    }
    return (0.7 / WBE_PI) * p_light.color * p_light.intensity * cone_factor * range_factor * range_factor * normal_factor;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    ConstantBuffer<DDGIConfig> config = ResourceDescriptorHeap[descriptor_indices.ddgi_config.x];
    ConstantBuffer<DDGITimeInfo> time_info = ResourceDescriptorHeap[descriptor_indices.ddgi_time_info.x];
    ConstantBuffer<SpotLightInfo> light_info = ResourceDescriptorHeap[descriptor_indices.spot_light_info.x];
    RaytracingAccelerationStructure acceleration_structure = ResourceDescriptorHeap[descriptor_indices.static_opaque_tlas.x];
    RWTexture2D<float4> probe_rays = ResourceDescriptorHeap[descriptor_indices.probe_rays.x];
    uint ray_index = p_dispatch_thread_id.x;
    uint probe_index = p_dispatch_thread_id.y;
    if (ray_index >= DDGI_RAYS_PER_PROBE || probe_index >= config.grid_counts_and_probe_count.w) {
        return;
    }
    float3 ray_direction = ddgi_ray_direction(ray_index, time_info.ray_rotation_seed);
    RayDesc ray;
    ray.Origin = ddgi_probe_position(config, probe_index);
    ray.Direction = ray_direction;
    ray.TMin = 0.03;
    ray.TMax = ddgi_max_ray_distance(config);
    float distance_to_hit = ddgi_max_ray_distance(config);
    float3 radiance = ddgi_sky_radiance(ray_direction);
    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(acceleration_structure, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed()) {
    }
    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        distance_to_hit = query.CommittedRayT();
        float3 hit_position = ray.Origin + ray_direction * distance_to_hit;
        radiance = ddgi_shade_hit(acceleration_structure, light_info, hit_position, -ray_direction);
    }
    probe_rays[uint2(ray_index, probe_index)] = float4(radiance, distance_to_hit);
}
