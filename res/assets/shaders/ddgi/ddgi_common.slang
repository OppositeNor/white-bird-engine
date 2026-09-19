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
#ifndef WBE_FILE_DDGI_COMMON_SLANG
#define WBE_FILE_DDGI_COMMON_SLANG

struct DDGIConfig {
    uint4 grid_counts_and_probe_count;
    float4 cell_spacing_and_max_ray_distance;
    float4 grid_origin_and_normal_bias;
    float4 probe_volume_extent_and_view_bias;
    float4 indirect_scale_and_hysteresis;
};

struct DDGITimeInfo {
    uint frame_index;
    uint ray_rotation_seed;
    float delta_seconds;
    float elapsed_seconds;
};

static const uint DDGI_RAYS_PER_PROBE = 8;
static const uint DDGI_IRRADIANCE_RESOLUTION = 8;
static const uint DDGI_DISTANCE_RESOLUTION = 16;

uint3 ddgi_grid_counts(DDGIConfig p_config) {
    return p_config.grid_counts_and_probe_count.xyz;
}

float3 ddgi_cell_spacing(DDGIConfig p_config) {
    return p_config.cell_spacing_and_max_ray_distance.xyz;
}

float3 ddgi_grid_origin(DDGIConfig p_config) {
    return p_config.grid_origin_and_normal_bias.xyz;
}

float ddgi_normal_bias(DDGIConfig p_config) {
    return p_config.grid_origin_and_normal_bias.w;
}

float3 ddgi_probe_volume_extent(DDGIConfig p_config) {
    return p_config.probe_volume_extent_and_view_bias.xyz;
}

float ddgi_view_bias(DDGIConfig p_config) {
    return p_config.probe_volume_extent_and_view_bias.w;
}

float ddgi_indirect_scale(DDGIConfig p_config) {
    return p_config.indirect_scale_and_hysteresis.x;
}

float ddgi_max_ray_distance(DDGIConfig p_config) {
    return p_config.cell_spacing_and_max_ray_distance.w;
}

uint ddgi_probe_index(DDGIConfig p_config, uint3 p_grid_coordinates) {
    uint3 counts = ddgi_grid_counts(p_config);
    return p_grid_coordinates.x + counts.x * (p_grid_coordinates.y + counts.y * p_grid_coordinates.z);
}

float3 ddgi_probe_position(DDGIConfig p_config, uint p_probe_index) {
    uint3 counts = ddgi_grid_counts(p_config);
    uint x = p_probe_index % counts.x;
    uint y = (p_probe_index / counts.x) % counts.y;
    uint z = p_probe_index / (counts.x * counts.y);
    return ddgi_grid_origin(p_config) + float3(x, y, z) * ddgi_cell_spacing(p_config);
}

float3 ddgi_ray_direction(uint p_ray_index, uint p_rotation_seed) {
    float index = float(p_ray_index) + 0.5;
    float y = 1.0 - 2.0 * index / float(DDGI_RAYS_PER_PROBE);
    float radius = sqrt(max(0.0, 1.0 - y * y));
    float rotation = float(p_rotation_seed & 65535U) * (2.0 * WBE_PI / 65536.0);
    float angle = index * 2.39996323 + rotation;
    return float3(cos(angle) * radius, y, sin(angle) * radius);
}

float3 ddgi_octahedron_decode(float2 p_uv) {
    float2 coordinates = p_uv * 2.0 - 1.0;
    float3 direction = float3(coordinates, 1.0 - abs(coordinates.x) - abs(coordinates.y));
    if (direction.z < 0.0) {
        direction.xy = (1.0 - abs(direction.yx)) * sign(direction.xy);
    }
    return normalize(direction);
}

float2 ddgi_octahedron_encode(float3 p_direction) {
    float3 direction = p_direction / max(abs(p_direction.x) + abs(p_direction.y) + abs(p_direction.z), 0.0001);
    if (direction.z < 0.0) {
        direction.xy = (1.0 - abs(direction.yx)) * sign(direction.xy);
    }
    return direction.xy * 0.5 + 0.5;
}

uint2 ddgi_atlas_pixel(DDGIConfig p_config, uint p_probe_index, uint2 p_local_pixel, uint p_resolution) {
    uint probes_per_row = ddgi_grid_counts(p_config).x * ddgi_grid_counts(p_config).y;
    return uint2(p_probe_index % probes_per_row, p_probe_index / probes_per_row) * p_resolution + p_local_pixel;
}

float3 ddgi_sample_irradiance(DDGIConfig p_config,
    Texture2D<float4> p_irradiance_atlas,
    Texture2D<float2> p_distance_atlas,
    float3 p_world_position,
    float3 p_normal,
    float3 p_view_direction) {
    float3 sample_position = ddgi_compute_biased_world_position(p_config, p_world_position, p_normal, p_view_direction);
    float3 normalized_coordinates;
    if (!ddgi_try_compute_normalized_probe_coordinates(p_config, sample_position, normalized_coordinates)) {
        return float3(0.0);
    }
    uint3 counts = ddgi_grid_counts(p_config);
    float3 grid_position = (sample_position - ddgi_grid_origin(p_config)) / ddgi_cell_spacing(p_config);
    uint3 cell = min(uint3(floor(grid_position)), counts - 2U);
    float3 fraction = saturate(grid_position - float3(cell));
    float3 weighted_irradiance = float3(0.0);
    float total_weight = 0.0;
    for (uint z = 0; z < 2; ++z) {
        for (uint y = 0; y < 2; ++y) {
            for (uint x = 0; x < 2; ++x) {
                uint3 corner = uint3(x, y, z);
                uint probe_index = ddgi_probe_index(p_config, cell + corner);
                float3 probe_position = ddgi_probe_position(p_config, probe_index);
                float3 to_probe = probe_position - p_world_position;
                float distance_to_probe = max(length(to_probe), 0.0001);
                float3 probe_direction = to_probe / distance_to_probe;
                float backface_weight = saturate(dot(p_normal, probe_direction) * 0.5 + 0.5);
                float3 trilinear = lerp(1.0 - fraction, fraction, float3(corner));
                float weight = trilinear.x * trilinear.y * trilinear.z * max(backface_weight, 0.05);
                float3 lookup_direction = (p_world_position - probe_position) / distance_to_probe;
                float2 octa_uv = ddgi_octahedron_encode(lookup_direction);
                uint2 distance_pixel = min(uint2(octa_uv * DDGI_DISTANCE_RESOLUTION), DDGI_DISTANCE_RESOLUTION - 1U);
                float2 moments = p_distance_atlas.Load(int3(ddgi_atlas_pixel(p_config, probe_index, distance_pixel, DDGI_DISTANCE_RESOLUTION), 0));
                float mean = moments.x;
                float variance = max(moments.y - mean * mean, 0.0001);
                float excess_distance = max(distance_to_probe - mean - ddgi_normal_bias(p_config), 0.0);
                float visibility = excess_distance > 0.0 ? variance / (variance + excess_distance * excess_distance) : 1.0;
                weight *= max(visibility * visibility, 0.02);
                uint2 irradiance_pixel = min(uint2(ddgi_octahedron_encode(p_normal) * DDGI_IRRADIANCE_RESOLUTION), DDGI_IRRADIANCE_RESOLUTION - 1U);
                float3 irradiance = p_irradiance_atlas.Load(int3(ddgi_atlas_pixel(p_config, probe_index, irradiance_pixel, DDGI_IRRADIANCE_RESOLUTION), 0)).rgb;
                weighted_irradiance += irradiance * weight;
                total_weight += weight;
            }
        }
    }
    return total_weight > 0.0 ? weighted_irradiance / total_weight : float3(0.0);
}

float3 ddgi_compute_biased_world_position(
    DDGIConfig p_config, float3 p_world_position, float3 p_normal, float3 p_view_direction) {
    return p_world_position + p_normal * ddgi_normal_bias(p_config) + p_view_direction * ddgi_view_bias(p_config);
}

bool ddgi_try_compute_normalized_probe_coordinates(DDGIConfig p_config, float3 p_world_position, out float3 p_normalized_coordinates) {
    float3 extent = ddgi_probe_volume_extent(p_config);
    float3 minimum_position = ddgi_grid_origin(p_config);
    float3 maximum_position = minimum_position + extent;
    if (any(p_world_position < minimum_position) || any(p_world_position > maximum_position)) {
        p_normalized_coordinates = float3(0.0);
        return false;
    }
    p_normalized_coordinates = saturate((p_world_position - minimum_position) / max(extent, float3(0.0001)));
    return true;
}

#endif
