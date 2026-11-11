// ============================================================================
// ZeGFX — Fast Approximate Anti-Aliasing (FXAA) Shader (DirectX 12)
// High-performance screen-space edge antialiasing fallback filter.
// ============================================================================

cbuffer FxaaConstants : register(b0)
{
    float viewportWidth;
    float viewportHeight;
    float subpixelQuality; // e.g. 0.75
    float edgeThreshold;   // e.g. 0.166
    float edgeThresholdMin;// e.g. 0.0833
    float padding0;
    float padding1;
    float padding2;
};

Texture2D inputTex : register(t0);
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

float rgbToLuma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 rcpFrame = float2(1.0f / max(viewportWidth, 1.0f), 1.0f / max(viewportHeight, 1.0f));

    float3 rgbM = inputTex.Sample(linearSampler, input.uv).rgb;
    float lumaM = rgbToLuma(rgbM);

    float lumaN = rgbToLuma(inputTex.Sample(linearSampler, input.uv + float2(0.0f, -rcpFrame.y)).rgb);
    float lumaS = rgbToLuma(inputTex.Sample(linearSampler, input.uv + float2(0.0f,  rcpFrame.y)).rgb);
    float lumaW = rgbToLuma(inputTex.Sample(linearSampler, input.uv + float2(-rcpFrame.x, 0.0f)).rgb);
    float lumaE = rgbToLuma(inputTex.Sample(linearSampler, input.uv + float2( rcpFrame.x, 0.0f)).rgb);

    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float lumaRange = lumaMax - lumaMin;

    if (lumaRange < max(edgeThresholdMin, lumaMax * edgeThreshold))
    {
        return float4(rgbM, 1.0f);
    }

    float lumaNW = rgbToLuma(inputTex.Sample(linearSampler, input.uv + float2(-rcpFrame.x, -rcpFrame.y)).rgb);
    float lumaNE = rgbToLuma(inputTex.Sample(linearSampler, input.uv + float2( rcpFrame.x, -rcpFrame.y)).rgb);
    float lumaSW = rgbToLuma(inputTex.Sample(linearSampler, input.uv + float2(-rcpFrame.x,  rcpFrame.y)).rgb);
    float lumaSE = rgbToLuma(inputTex.Sample(linearSampler, input.uv + float2( rcpFrame.x,  rcpFrame.y)).rgb);

    float edgeVert = abs((0.25f * lumaNW) + (-0.5f * lumaN) + (0.25f * lumaNE)) +
                     abs((0.50f * lumaW ) + (-1.0f * lumaM) + (0.50f * lumaE )) +
                     abs((0.25f * lumaSW) + (-0.5f * lumaS) + (0.25f * lumaSE));
    float edgeHorz = abs((0.25f * lumaNW) + (-0.5f * lumaW) + (0.25f * lumaSW)) +
                     abs((0.50f * lumaN ) + (-1.0f * lumaM) + (0.50f * lumaS )) +
                     abs((0.25f * lumaNE) + (-0.5f * lumaE) + (0.25f * lumaSE));
    bool isHorizontal = (edgeHorz >= edgeVert);

    float luma1 = isHorizontal ? lumaN : lumaW;
    float luma2 = isHorizontal ? lumaS : lumaE;
    float gradient1 = abs(luma1 - lumaM);
    float gradient2 = abs(luma2 - lumaM);

    bool is1Steeper = (gradient1 >= gradient2);
    float gradientScaled = 0.25f * max(gradient1, gradient2);

    float stepLength = isHorizontal ? rcpFrame.y : rcpFrame.x;
    if (is1Steeper) stepLength = -stepLength;

    float2 currentUv = input.uv;
    if (isHorizontal)
        currentUv.y += stepLength * 0.5f;
    else
        currentUv.x += stepLength * 0.5f;

    float2 offset = isHorizontal ? float2(rcpFrame.x, 0.0f) : float2(0.0f, rcpFrame.y);
    float3 rgbA = 0.5f * (inputTex.Sample(linearSampler, currentUv - offset * 0.333f).rgb +
                          inputTex.Sample(linearSampler, currentUv + offset * 0.333f).rgb);
    float3 rgbB = rgbA * 0.5f + 0.25f * (inputTex.Sample(linearSampler, currentUv - offset * 1.0f).rgb +
                                         inputTex.Sample(linearSampler, currentUv + offset * 1.0f).rgb);

    float lumaB = rgbToLuma(rgbB);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        return float4(rgbA, 1.0f);
    }
    return float4(rgbB, 1.0f);
}
