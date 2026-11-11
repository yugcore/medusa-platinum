#include "../common/bloom_common.hlsli"

float4 PSMain(VSOutput input) : SV_TARGET
{
    float2 texel = 1.0f / max(sourceSize, 1.0f.xx);
    float3 color = SampleBloomBox13(sourceTexture, input.uv, texel);
    color = all(isfinite(color)) ? max(color, 0.0f) : 0.0f;
    return float4(color, 1.0f);
}
