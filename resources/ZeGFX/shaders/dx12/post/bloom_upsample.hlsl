#include "../common/bloom_common.hlsli"

float4 PSMain(VSOutput input) : SV_TARGET
{
    float spread = lerp(0.75f, 2.5f, saturate(radius * 0.5f));
    float scatter = lerp(0.55f, 0.90f, saturate(radius * 0.5f));
    float2 texel = spread / max(sourceSize, 1.0f.xx);
    float3 lowFrequency =
        SampleBloomTent9(sourceTexture, input.uv, texel);
    float3 highFrequency =
        detailTexture.Sample(linearClampSampler, input.uv).rgb;
    float3 color = highFrequency + lowFrequency * scatter;
    color = all(isfinite(color)) ? max(color, 0.0f) : 0.0f;
    return float4(color, 1.0f);
}
