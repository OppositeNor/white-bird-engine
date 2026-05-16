// vertex
#include "forward_cook_torrance_pbr/forward_cook_torrance_pbr_utils.slang"
#include "defs.slang"

[[vk::binding(0, 0)]] ConstantBuffer<CameraInfo> camera_info;

VSOutput main(Vertex3DTexturedInput p_input) {
    VSOutput output;
    float4 world_position = float4(p_input.position.xyz, 1.0);
    output.position = mul(camera_info.proj, mul(camera_info.view, world_position));
    output.world_position = p_input.position;
    output.normal = p_input.normal;
    output.tangent = p_input.tangent;
    output.bitangent = p_input.bitangent;
    output.uv = p_input.uv;
    return output;
}