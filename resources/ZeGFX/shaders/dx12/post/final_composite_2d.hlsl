cbuffer FrameConstants : register(b0)
{
    float viewportWidth;
    float viewportHeight;
    float exposure;
    float whitePoint;
    float bloomIntensity;
    uint bloomEnabled;
    uint debugMode;
    float padding0;
    float contrast;
    float saturation;
    float temperature;
    float tint;
    uint toneMapOperator;
    float3 padding1;
};

Texture2D sceneColorTex : register(t0);
Texture2D bloomTex : register(t1);
Texture2D<float4> adaptiveExposureTex : register(t2);
SamplerState linearSampler : register(s0);

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
    float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float2 ndc;
    ndc.x = (input.pos.x / viewportWidth) * 2.0f - 1.0f;
    ndc.y = 1.0f - (input.pos.y / viewportHeight) * 2.0f;
    output.pos = float4(ndc, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float3 acesApprox(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 hdr = max(sceneColorTex.Sample(linearSampler, input.uv).rgb, 0.0f);

    // Scalar AO diagnostics are intentionally displayed without exposure,
    // bloom, ACES, or gamma reshaping.
    if (debugMode >= 7u && debugMode <= 10u)
    {
        return float4(saturate(hdr), 1.0f) * input.color;
    }

    // HDR and bloom debug buffers use an explicit diagnostic display mapping,
    // not the production ACES curve. This keeps values above 1 visible while
    // avoiding the false impression that the debug texture itself is LDR.
    if (debugMode >= 12u && debugMode <= 21u)
    {
        // Emissive uses a lower scale so a 0/2/5/10 strength ramp remains
        // distinguishable instead of all four values clipping to white.
        float diagnosticScale = debugMode == 16u ? 0.08f : 0.35f;
        float3 diagnostic = 1.0f - exp(-hdr * diagnosticScale);
        diagnostic = pow(saturate(diagnostic), 1.0f / 2.2f);
        return float4(diagnostic, 1.0f) * input.color;
    }

    // The production order is deliberately:
    // linear HDR SceneColor -> reconstructed bloom composite -> exposure ->
    // HDR grading -> tone mapping -> display gamma.
    // Bloom therefore reacts to unclipped highlights.
    if (bloomEnabled != 0u)
    {
        hdr += max(bloomTex.Sample(linearSampler, input.uv).rgb, 0.0f) *
               max(bloomIntensity, 0.0f);
    }

    hdr *= max(padding0>.5f?adaptiveExposureTex.Load(int3(0,0,0)).r:exposure,0.001f);
    float luma = dot(hdr, float3(0.2126f, 0.7152f, 0.0722f));
    hdr = lerp(luma.xxx, hdr, saturation);
    // 18% linear-grey pivot keeps contrast physically useful before tone mapping.
    hdr = max((hdr - 0.18f) * contrast + 0.18f, 0.0f);
    hdr += float3(temperature * 0.05f, tint * 0.02f, -temperature * 0.05f);
    hdr = max(hdr, 0.0f); // Preserve highlights above 1 throughout grading.
    float3 scaled = hdr / max(whitePoint, 0.001f);

    float3 mapped;
    if (toneMapOperator == 0u) mapped = saturate(scaled);
    else if (toneMapOperator == 1u) mapped = scaled / (1.0f + scaled);
    else if (toneMapOperator == 2u) {
        float3 x = max(0.0f, scaled - 0.004f);
        // The filmic fit includes display encoding; undo it before the shared gamma step.
        mapped = pow((x * (6.2f * x + 0.5f)) / (x * (6.2f * x + 1.7f) + 0.06f), 2.2f);
    } else mapped = acesApprox(scaled);
    mapped = pow(saturate(mapped), 1.0f / 2.2f);
    return float4(mapped, 1.0f) * input.color;
}
