cbuffer AOConstants : register(b0)
{
    row_major float4x4 inverseViewProjection;
    float4 cameraPosition;
    float2 viewportSize;
    float2 inverseViewportSize;
    float aoRadius;
    float aoIntensity;
    float aoBias;
    float aoPower;
    float aoMinVisibility;
    float projectionScale;
    uint aoSampleCount;
    float blurDepthThreshold;
    float blurNormalThreshold;
    float3 aoPadding;
};

Texture2D<float> rawAoTex : register(t0);
Texture2D<float> depthTex : register(t1);
Texture2D<float4> normalTex : register(t2);
SamplerState pointSampler : register(s0);

struct VSInput
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float2 ndc;
    ndc.x = (input.pos.x / viewportSize.x) * 2.0f - 1.0f;
    ndc.y = 1.0f - (input.pos.y / viewportSize.y) * 2.0f;
    output.pos = float4(ndc, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float4 clip = float4(
        uv.x * 2.0f - 1.0f,
        1.0f - uv.y * 2.0f,
        depth,
        1.0f);
    clip.xy -= aoPadding.xy;
    float4 world = mul(clip, inverseViewProjection);
    return world.xyz / max(abs(world.w), 0.00001f);
}

float PSMain(PSInput input) : SV_TARGET
{
    const int2 centerPixel = int2(input.pos.xy);
    const float centerDepth = depthTex.Load(int3(centerPixel, 0));
    if (centerDepth >= 0.999999f)
    {
        return 1.0f;
    }

    const float centerAo = rawAoTex.Load(int3(centerPixel, 0));
    const float3 centerNormal =
        normalize(normalTex.Load(int3(centerPixel, 0)).xyz * (aoPadding.z > .5f ? 2 : 1) - (aoPadding.z > .5f ? 1 : 0));
    const float3 centerPosition =
        ReconstructWorldPosition(input.uv, centerDepth);
    const float centerViewDistance =
        length(centerPosition - cameraPosition.xyz);

    float weightedAo = centerAo;
    float totalWeight = 1.0f;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            if (x == 0 && y == 0)
            {
                continue;
            }

            const float2 offset = float2(x, y);
            const float2 sampleUv =
                input.uv + offset * inverseViewportSize;
            const float sampleDepth =
                depthTex.SampleLevel(pointSampler, sampleUv, 0);
            if (sampleDepth >= 0.999999f)
            {
                continue;
            }

            const float3 sampleNormal = normalize(
                normalTex.SampleLevel(pointSampler, sampleUv, 0).xyz * (aoPadding.z > .5f ? 2 : 1) - (aoPadding.z > .5f ? 1 : 0));
            const float3 samplePosition =
                ReconstructWorldPosition(sampleUv, sampleDepth);
            const float sampleViewDistance =
                length(samplePosition - cameraPosition.xyz);

            const float spatialWeight =
                exp(-dot(offset, offset) * 0.18f);
            const float depthDelta =
                abs(sampleViewDistance - centerViewDistance);
            const float depthWeight = exp(
                -depthDelta /
                max(blurDepthThreshold, aoRadius * 0.08f));
            const float normalAgreement =
                saturate(dot(centerNormal, sampleNormal));
            const float normalWeight = smoothstep(
                blurNormalThreshold,
                1.0f,
                normalAgreement);
            const float weight =
                spatialWeight * depthWeight * normalWeight;

            weightedAo +=
                rawAoTex.SampleLevel(pointSampler, sampleUv, 0) *
                weight;
            totalWeight += weight;
        }
    }

    return saturate(weightedAo / max(totalWeight, 0.0001f));
}
