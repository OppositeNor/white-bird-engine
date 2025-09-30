// ps_6_0
#include "base_pipeline.hlsl"

float4 main(PSInput input) : SV_Target {
    return float4(input.color, 1.0);
}

