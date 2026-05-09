#ifndef WBE_FILE_FORWARD_COOK_TORRANCE_PBR_UTILS_SLANG
#define WBE_FILE_FORWARD_COOK_TORRANCE_PBR_UTILS_SLANG

static const float WBE_PI = 3.14159265359;

struct VSOutput {
    float4 position       : SV_Position;
    float3 world_position : TEXCOORD0;
    float3 normal         : TEXCOORD1;
    float3 tangent        : TEXCOORD2;
    float3 bitangent      : TEXCOORD3;
    float2 uv             : TEXCOORD4;
};

float3 srgb_to_linear(float3 p_color) {
    return pow(max(p_color, float3(0.0)), float3(2.2));
}

float3 linear_to_srgb(float3 p_color) {
    return pow(max(p_color, float3(0.0)), float3(1.0 / 2.2));
}

float distribution_ggx(float3 p_normal, float3 p_half_vector, float p_roughness) {
    float alpha = p_roughness * p_roughness;
    float alpha2 = alpha * alpha;
    float ndoth = max(dot(p_normal, p_half_vector), 0.0);
    float ndoth2 = ndoth * ndoth;
    float denominator = ndoth2 * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(WBE_PI * denominator * denominator, 0.0001);
}

float geometry_schlick_ggx(float p_ndotv, float p_roughness) {
    float k = (p_roughness + 1.0) * (p_roughness + 1.0) / 8.0;
    return p_ndotv / max(p_ndotv * (1.0 - k) + k, 0.0001);
}

float geometry_smith(float3 p_normal, float3 p_view_dir, float3 p_light_dir, float p_roughness) {
    float ndotv = max(dot(p_normal, p_view_dir), 0.0);
    float ndotl = max(dot(p_normal, p_light_dir), 0.0);
    return geometry_schlick_ggx(ndotv, p_roughness) * geometry_schlick_ggx(ndotl, p_roughness);
}

float3 fresnel_schlick(float p_cos_theta, float3 p_f0) {
    return p_f0 + (1.0 - p_f0) * pow(1.0 - clamp(p_cos_theta, 0.0, 1.0), 5.0);
}

float3 normal_from_texture(float3 p_sampled_normal, float3 p_normal, float3 p_tangent, float3 p_bitangent) {
    float3 normal = normalize(p_normal);
    float3 tangent = p_tangent;
    float3 bitangent = p_bitangent;
    if (dot(tangent, tangent) < 0.0001 || dot(bitangent, bitangent) < 0.0001) {
        float3 up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
        tangent = normalize(cross(up, normal));
        bitangent = normalize(cross(normal, tangent));
    } else {
        tangent = normalize(tangent - normal * dot(normal, tangent));
        bitangent = normalize(bitangent);
    }

    float3 texture_normal = p_sampled_normal * 2.0 - 1.0;
    return normalize(tangent * texture_normal.x + bitangent * texture_normal.y + normal * texture_normal.z);
}

#endif