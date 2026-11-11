struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer SSRConstants : register(b0)
{
    float2 screenSize;
    float2 texelSize;
    float ssrMaxDistance;
    float ssrThickness;
    float ssrStride;
    int ssrBinarySearchSteps;
    float ssrFallbackIntensity;
    float3 ssrPadding;
};

Texture2D sceneColorTex   : register(t0);
Texture2D depthNormalTex  : register(t1);
SamplerState linearSampler : register(s1);

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 dn = depthNormalTex.Sample(linearSampler, input.uv);
    float3 normal = normalize(dn.xyz * 2.0f - 1.0f);
    float depth = dn.w;
    
    if (depth >= 1.0f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Reconstruct view direction from UV + depth
    float3 viewDir = normalize(float3(input.uv * 2.0f - 1.0f, 1.0f));
    float3 reflectDir = reflect(viewDir, normal);
    
    // Ray march in screen space
    float2 currentUV = input.uv;
    float currentDepth = depth;
    float stepSize = ssrStride * texelSize.x;
    
    for (int step = 0; step < 64; ++step) {
        currentUV += reflectDir.xy * stepSize;
        
        if (currentUV.x < 0.0f || currentUV.x > 1.0f ||
            currentUV.y < 0.0f || currentUV.y > 1.0f) break;
        
        float sampleDepth = depthNormalTex.Sample(linearSampler, currentUV).w;
        float depthDelta = currentDepth + reflectDir.z * stepSize * (float)(step + 1) - sampleDepth;
        
        if (depthDelta > 0.0f && depthDelta < ssrThickness) {
            // Binary search refinement
            float2 hitUV = currentUV;
            for (int b = 0; b < ssrBinarySearchSteps; ++b) {
                stepSize *= 0.5f;
                float2 midUV = hitUV - reflectDir.xy * stepSize;
                float midDepth = depthNormalTex.Sample(linearSampler, midUV).w;
                float midDelta = currentDepth + reflectDir.z * stepSize - midDepth;
                if (midDelta > 0.0f) hitUV = midUV;
            }
            
            float3 hitColor = sceneColorTex.Sample(linearSampler, hitUV).rgb;
            float edgeFade = 1.0f - pow(max(abs(hitUV.x * 2.0f - 1.0f), abs(hitUV.y * 2.0f - 1.0f)), 4.0f);
            return float4(hitColor * edgeFade, edgeFade);
        }
    }
    
    // Miss — return fallback color (black for now, fallback blended in composite)
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
