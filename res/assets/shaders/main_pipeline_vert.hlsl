// vert
#include "main_pipeline_utils.hlslh"

VSOutput main(uint p_vertex_id : SV_VertexID) {
    VSOutput output;

    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    float2 uvs[3] = {
        float2(0.0, 0.0),
        float2(0.0, 2.0),
        float2(2.0, 0.0)
    };

    output.position = float4(positions[p_vertex_id], 0.0, 1.0);
    output.uv       = uvs[p_vertex_id];

    return output;
}

