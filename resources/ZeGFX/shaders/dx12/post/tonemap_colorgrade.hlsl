struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer ToneMapConstants : register(b0)
{
    float exposure;
    float whitePoint;
    int toneMapOperator;   // 0: linear, 1: reinhard, 2: filmic, 3: aces
    float gammaValue;
    
    float contrast;
    float saturation;
    float temperature;
    float tint;
    
    float bloomEnabled;
    float bloomMix;
    float2 tmPadding;
};

Texture2D sceneColorTex : register(t0);
Texture2D bloomTex      : register(t1);
SamplerState linearSampler : register(s1);

float3 reinhard(float3 x) {
    return x / (x + 1.0f);
}

float3 filmic(float3 x) {
    x = max(float3(0.0f, 0.0f, 0.0f), x - 0.004f);
    return (x * (6.2f * x + 0.5f)) / (x * (6.2f * x + 1.7f) + 0.06f);
}

float3 acesApprox(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Color grading runs in LINEAR HDR space, before tone mapping.
// This preserves full dynamic range — contrast and saturation affect
// highlights and shadows correctly instead of crushing clipped values.
float3 colorGrade(float3 color) {
    // Saturation via luma-weighted desaturate
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(float3(luma, luma, luma), color, saturation);

    // Contrast pivoted around 18% linear grey (not 0.5 which is post-gamma space).
    // Avoids crushing shadows when contrast > 1.
    const float MID = 0.18f;
    color = (color - MID) * contrast + MID;
    color = max(color, 0.0f); // clamp negatives, keep HDR range intact

    // Temperature: positive = warm (golden hour), negative = cool (overcast).
    // Scale 0.05 gives ~1 stop of perceptible shift at value 10.
    color.r += temperature * 0.05f;
    color.b -= temperature * 0.05f;

    // Tint: green (+) / magenta (-) axis
    color.g += tint * 0.02f;

    return max(color, 0.0f); // do NOT saturate — preserve HDR for tonemapper
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float3 hdrColor = sceneColorTex.Sample(linearSampler, input.uv).rgb;
    
    // Add bloom
    if (bloomEnabled > 0.5f) {
        float3 bloom = bloomTex.Sample(linearSampler, input.uv).rgb;
        hdrColor += bloom * bloomMix;
    }
    
    // Apply exposure
    hdrColor *= exposure;
    
    // Color grade
    hdrColor = colorGrade(hdrColor);

    // Scale by whitePoint before tonemapping so you can control highlight rolloff.
    // whitePoint = 1.0 is neutral; raise to 2-4 to recover blown highlights.
    float3 scaledColor = hdrColor / max(whitePoint, 0.001f);

    // Tone map
    float3 mapped;
    if (toneMapOperator == 0) {
        mapped = saturate(scaledColor);
    } else if (toneMapOperator == 1) {
        mapped = reinhard(scaledColor);
    } else if (toneMapOperator == 2) {
        mapped = filmic(scaledColor);
    } else {
        // ACES — most cinematic, best for outdoor golden-hour scenes
        mapped = acesApprox(scaledColor);
    }

    // Gamma: clamp gamma to safe range to avoid pow(0, negative) NaN
    float safeGamma = max(gammaValue, 0.001f);
    mapped = pow(saturate(mapped), 1.0f / safeGamma);

    return float4(mapped, 1.0f);
}
