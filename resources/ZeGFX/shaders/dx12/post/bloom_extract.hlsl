#include "../common/bloom_common.hlsli"

float3 ApplyBloomSoftThreshold(float3 color)
{
    color = all(isfinite(color)) ? max(color, 0.0f) : 0.0f;
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float knee = max(threshold * 0.5f, 0.0001f);
    float soft = clamp(luminance - threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 0.0001f);
    float contribution = max(luminance - threshold, soft);
    return color * (contribution / max(luminance, 0.0001f));
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // SceneColor is sampled before exposure, ACES, and display gamma.
    float2 texel = 1.0f / max(sourceSize, 1.0f.xx);
    float3 filtered = SampleBloomBox13(sourceTexture, input.uv, texel);
    return float4(ApplyBloomSoftThreshold(filtered), 1.0f);
}
