Texture2D<float4> GBufferNormal : register(t0);
Texture2D<float4> GBufferMaterial : register(t1); // roughness, metallic
Texture2D<float>  SceneDepth : register(t2);
TextureCube       GlobalReflectionProbe : register(t3);
SamplerState      PointSampler : register(s0);
SamplerState      LinearSampler : register(s1);

cbuffer ReflectionParams : register(b0) {
    float MaxRoughness;
    uint  MaxMipLevel;
    float2 ScreenResolution;
};

float TraceScreenSpaceReflections(float2 uv, float3 reflectionDir, out float3 ssrColor) {
    ssrColor = float3(0, 0, 0);
    // Ray marching screen depth placeholder
    return 0.0f; // Return weight (0 if no hit)
}

float3 ComputeReflections(float2 uv, float3 viewDir) {
    float roughness = GBufferMaterial[uv].r;
    float3 normal = GBufferNormal[uv].xyz;
    
    float3 reflectionDir = reflect(-viewDir, normal);
    float3 ssrColor = float3(0, 0, 0);
    float ssrWeight = 0.0f;

    // 1. Attempt Screen-Space Reflection
    if (roughness < MaxRoughness) {
        ssrWeight = TraceScreenSpaceReflections(uv, reflectionDir, ssrColor);
    }

    // 2. Sample Static/Dynamic Specular Probes
    float3 probeColor = GlobalReflectionProbe.SampleLevel(LinearSampler, reflectionDir, roughness * MaxMipLevel).rgb;

    // 3. Composite with roughness-aware interpolation
    float3 finalReflection = lerp(probeColor, ssrColor, ssrWeight);
    return finalReflection;
}

[numthreads(16, 16, 1)]
void ReflectionCS(uint3 DispatchThreadID : SV_DispatchThreadID) {
    if (DispatchThreadID.x >= (uint)ScreenResolution.x || DispatchThreadID.y >= (uint)ScreenResolution.y) {
        return;
    }
    float2 uv = float2(DispatchThreadID.xy) / ScreenResolution;
    float3 viewDir = float3(0, 0, -1);
    float3 color = ComputeReflections(uv, viewDir);
}
