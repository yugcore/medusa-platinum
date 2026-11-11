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

// The raw pass uses t1/t2 directly. t0 is retained so the raw and blur
// pipelines share one compact root signature.
Texture2D<float> primaryInput : register(t0);
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

static const float2 kSampleDisk[32] =
{
    float2( 0.1768f,  0.1768f), float2(-0.1768f, -0.1768f),
    float2(-0.3062f,  0.3062f), float2( 0.3062f, -0.3062f),
    float2( 0.5590f,  0.0000f), float2(-0.5590f,  0.0000f),
    float2( 0.0000f,  0.6614f), float2( 0.0000f, -0.6614f),
    float2( 0.5590f,  0.5590f), float2(-0.5590f, -0.5590f),
    float2(-0.6200f,  0.3600f), float2( 0.6200f, -0.3600f),
    float2( 0.2600f,  0.7800f), float2(-0.2600f, -0.7800f),
    float2(-0.8200f, -0.1800f), float2( 0.8200f,  0.1800f),
    float2( 0.4300f, -0.1300f), float2(-0.4300f,  0.1300f),
    float2( 0.1200f,  0.4700f), float2(-0.1200f, -0.4700f),
    float2(-0.3500f, -0.5200f), float2( 0.3500f,  0.5200f),
    float2( 0.6900f,  0.3100f), float2(-0.6900f, -0.3100f),
    float2(-0.1500f,  0.8800f), float2( 0.1500f, -0.8800f),
    float2( 0.9000f, -0.3900f), float2(-0.9000f,  0.3900f),
    float2( 0.5200f, -0.7200f), float2(-0.5200f,  0.7200f),
    float2(-0.7200f,  0.6500f), float2( 0.7200f, -0.6500f)
};

float PSMain(PSInput input) : SV_TARGET
{
    const int2 pixel = int2(input.pos.xy);
    const float centerDepth = depthTex.Load(int3(pixel, 0));
    if (centerDepth >= 0.999999f)
    {
        return 1.0f;
    }

    const float3 centerPosition =
        ReconstructWorldPosition(input.uv, centerDepth);
    const float3 centerNormal =
        normalize(normalTex.Load(int3(pixel, 0)).xyz * (aoPadding.z > .5f ? 2 : 1) - (aoPadding.z > .5f ? 1 : 0));
    const float viewDistance =
        max(length(centerPosition - cameraPosition.xyz), 0.05f);

    // Project the world-space radius to pixels. Keeping the kernel bounded
    // avoids both distant gray wash and pathological near-camera cost.
    const float pixelRadius = clamp(
        aoRadius * projectionScale * viewportSize.y *
            (0.5f / viewDistance),
        2.0f,
        96.0f);

    float occlusion = 0.0f;
    float validSamples = 0.0f;
    const uint sampleCount = clamp(aoSampleCount, 8u, 32u);

    [loop]
    for (uint sampleIndex = 0u; sampleIndex < sampleCount; ++sampleIndex)
    {
        // Point depth belongs to a texel centre. Reconstructing at the
        // fractional kernel UV makes a flat sloped surface occlude itself.
        const float2 kernelUv = input.uv + kSampleDisk[sampleIndex] * pixelRadius * inverseViewportSize;
        const float2 sampleUv = (floor(kernelUv * viewportSize) + 0.5f) * inverseViewportSize;
        if (any(sampleUv <= 0.0f) || any(sampleUv >= 1.0f))
        {
            continue;
        }

        const float sampleDepth =
            depthTex.SampleLevel(pointSampler, sampleUv, 0);
        if (sampleDepth >= 0.999999f)
        {
            continue;
        }

        const float3 samplePosition =
            ReconstructWorldPosition(sampleUv, sampleDepth);
        const float3 delta = samplePosition - centerPosition;
        const float distanceToSample = length(delta);
        if (distanceToSample <= 0.0001f ||
            distanceToSample >= aoRadius)
        {
            continue;
        }

        // A visible neighbour blocks the normal-facing hemisphere only when
        // it rises above the tangent plane. The bias rejects coplanar
        // self-occlusion; the range term gives radius a predictable falloff.
        const float normalDistance = dot(delta, centerNormal);
        const float horizon =
            saturate(
                (normalDistance - aoBias) /
                max(distanceToSample, 0.0001f));
        const float rangeWeight =
            saturate(1.0f - distanceToSample / max(aoRadius, 0.001f));
        occlusion += horizon * rangeWeight * rangeWeight;
        validSamples += 1.0f;
    }

    const float normalizedOcclusion =
        occlusion / max(validSamples, 1.0f);
    const float visibility = saturate(
        1.0f - normalizedOcclusion * aoIntensity * 3.0f);
    const float shaped = pow(visibility, max(aoPower, 0.25f));
    return max(shaped, saturate(aoMinVisibility));
}
