struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer BloomBlurConstants : register(b0)
{
    float2 blurDirection;  // (1,0) for H, (0,1) for V
    float2 texelSize;
};

Texture2D inputTex : register(t0);
SamplerState linearSampler : register(s1);

static const float weights[7] = { 0.0205, 0.0855, 0.232, 0.324, 0.232, 0.0855, 0.0205 };

float4 PSMain(VSOutput input) : SV_TARGET
{
    float3 result = float3(0.0f, 0.0f, 0.0f);
    for (int i = -3; i <= 3; ++i) {
        float2 offset = blurDirection * texelSize * (float)i;
        result += inputTex.Sample(linearSampler, input.uv + offset).rgb * weights[i + 3];
    }
    return float4(result, 1.0f);
}
