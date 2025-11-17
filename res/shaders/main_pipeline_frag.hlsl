// frag

struct PSInput {
    float2 uv : COLOR0;
};

float4 main(PSInput input) : SV_Target {
    return float4(input.uv, 0.0, 1.0);
}

