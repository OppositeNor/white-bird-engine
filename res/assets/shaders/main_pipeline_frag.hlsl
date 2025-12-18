// frag
#include "main_pipeline_utils.hlslh"

float4 main(VSOutput p_input) : SV_Target {
    return float4(p_input.uv, 0.0, 1.0);
}

