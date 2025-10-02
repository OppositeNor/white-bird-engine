// ps_6_0
struct PSInput {
    float3 color : COLOR0;
};

float4 main(PSInput input) : SV_Target {
    return float4(input.color, 1.0);
}

