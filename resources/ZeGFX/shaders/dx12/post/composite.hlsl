struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D sceneColorTex  : register(t0);
Texture2D aoTex          : register(t1);
Texture2D reflectionTex  : register(t2);
SamplerState linearSampler : register(s1);

cbuffer CompositeConstants : register(b0)
{
    float aoEnabled;
    float reflectionEnabled;
    float reflectionStrength;
    float debugMode; // 0 = Off, 1 = Overdraw Heatmap, 2 = Complexity Heatmap
};

float3 HeatmapColor(float t) {
    if (t < 0.25f) return lerp(float3(0,0,1), float3(0,1,1), t * 4.0f);
    if (t < 0.50f) return lerp(float3(0,1,1), float3(0,1,0), (t - 0.25f) * 4.0f);
    if (t < 0.75f) return lerp(float3(0,1,0), float3(1,1,0), (t - 0.50f) * 4.0f);
    return lerp(float3(1,1,0), float3(1,0,0), (t - 0.75f) * 4.0f);
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float3 sceneColor = sceneColorTex.Sample(linearSampler, input.uv).rgb;
    float ao = aoEnabled > 0.5f ? aoTex.Sample(linearSampler, input.uv).r : 1.0f;
    float4 reflection = reflectionTex.Sample(linearSampler, input.uv);
    
    if (debugMode > 0.5f) {
        float heatmapVal = clamp(sceneColor.r * 2.0f, 0.0f, 1.0f);
        return float4(HeatmapColor(heatmapVal), 1.0f);
    }

    // Apply AO to ambient/indirect term
    float3 result = sceneColor * ao;
    
    // Add reflection contribution
    if (reflectionEnabled > 0.5f) {
        result = lerp(result, reflection.rgb, reflection.a * reflectionStrength);
    }
    
    return float4(result, 1.0f);
}
