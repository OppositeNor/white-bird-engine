// vertex
#include "forward_cook_torrance_pbr/forward_cook_torrance_pbr_utils.slang"
#include "defs.slang"

/**
 * @brief Constant buffer providing the per-frame view and projection matrices.
 */
[[vk::binding(0, 0)]] ConstantBuffer<CameraInfo> camera_info;

/**
 * @brief Spot-light constant buffer. The vertex stage uses the view/proj matrices to compute the light-space position consumed by the fragment stage for shadow mapping.
 */
[[vk::binding(5, 0)]] ConstantBuffer<SpotLightInfo> spot_light_info;

/**
 * @brief Transform a textured vertex into clip space and pass per-vertex attributes through to the fragment stage.
 * @param p_input The incoming vertex attributes.
 * @return The vertex output for the forward Cook-Torrance PBR pass.
 */
VSOutput main(Vertex3DTexturedInput p_input) {
    VSOutput output;
    float4 world_position = float4(p_input.position.xyz, 1.0);
    output.position = mul(camera_info.proj, mul(camera_info.view, world_position));
    output.world_position = p_input.position;
    output.normal = p_input.normal;
    output.tangent = p_input.tangent;
    output.bitangent = p_input.bitangent;
    output.uv = p_input.uv;
    output.light_space_pos = mul(spot_light_info.proj, mul(spot_light_info.view, world_position));
    return output;
}
