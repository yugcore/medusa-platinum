#ifndef ZE_SHADOW_COMMON_HLSLI
#define ZE_SHADOW_COMMON_HLSLI

struct DirectionalShadowCascade {
    float4x4 worldToShadowClip;
    float4 atlasScaleBias;
    float splitNear;
    float splitFar;
    float texelWorldSize;
    float depthBias;
    float normalBias;
    uint resolution;
};

struct DirectionalShadowGpuCascade {
    row_major float4x4 worldToShadowClip;
    float4 atlasScaleBias;
    float4 splitDepthBias;
    float4 filterParams;
};

struct DirectionalShadowGpuData {
    DirectionalShadowGpuCascade cascades[4];
    float4 cascadeCountMaxDistanceBlendFilter;
};

// Computes bias based on surface normal relative to light direction
float GetShadowBias(float3 normal, float3 lightDir, float baseBias, float slopeScale) {
    float cosAlpha = saturate(dot(normal, lightDir));
    float tanAlpha = sqrt(1.0f - cosAlpha * cosAlpha) / max(cosAlpha, 0.0001f);
    return baseBias + tanAlpha * slopeScale * baseBias;
}

uint SelectShadowCascade(float viewDepth, DirectionalShadowGpuData shadowData) {
    uint cascadeCount = min((uint)shadowData.cascadeCountMaxDistanceBlendFilter.x, 4u);
    [unroll]
    for (uint i = 0; i < 4u; ++i) {
        if (i >= cascadeCount) break;
        if (viewDepth <= shadowData.cascades[i].splitDepthBias.y) {
            return i;
        }
    }
    return max(cascadeCount, 1u) - 1u;
}

// Perform standard 3x3 PCF filter on a shadow atlas
float SampleShadowPCF(Texture2D shadowMap, SamplerComparisonState cmpSampler, float3 uvDepth, float2 texelSize) {
    float shadow = 0.0f;
    [unroll]
    for (int x = -1; x <= 1; ++x) {
        [unroll]
        for (int y = -1; y <= 1; ++y) {
            float2 offset = float2(x, y) * texelSize;
            shadow += shadowMap.SampleCmpLevelZero(cmpSampler, uvDepth.xy + offset, uvDepth.z);
        }
    }
    return shadow / 9.0f;
}

#endif // ZE_SHADOW_COMMON_HLSLI
