#include "../common/ze_gi_common.hlsli"
#include "../common/ze_view_parameters.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D<float> DepthBuffer : register(t0);
Texture2D<float4> NormalMap : register(t1);
Texture2D<float4> IrradianceAtlas : register(t2);
Texture2D<float4> DistanceAtlas : register(t3);
SamplerState LinearSampler : register(s0);

cbuffer GatherCB : register(b0) {
    uint3 probeCounts;
    float3 probeSpacing;
    float3 volumeOrigin;
};

float3 GetViewPos(float2 uv, float depth) {
    float4 clipPos = float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f);
    float4 viewPos = mul(clipPos, inverseProjection);
    return viewPos.xyz / viewPos.w;
}

float4 PSMain(PSInput input) : SV_Target {
    float depth = DepthBuffer.Sample(LinearSampler, input.uv).r;
    if (depth >= 1.0f) {
        return float4(0.f, 0.f, 0.f, 1.0f);
    }
    
    float3 P = GetViewPos(input.uv, depth);
    float3 N = normalize(NormalMap.Sample(LinearSampler, input.uv).xyz * 2.f - 1.f);
    
    // Find grid position
    float3 gridPos = (P - volumeOrigin) / probeSpacing;
    int3 baseCoord = floor(gridPos);
    
    float3 accumulatedIrradiance = float3(0.f, 0.f, 0.f);
    float totalWeight = 0.0f;
    
    // Gather from 8 surrounding probes
    for (int z = 0; z <= 1; ++z) {
        for (int y = 0; y <= 1; ++y) {
            for (int x = 0; x <= 1; ++x) {
                int3 coord = baseCoord + int3(x, y, z);
                if (coord.x >= 0 && coord.x < (int)probeCounts.x &&
                    coord.y >= 0 && coord.y < (int)probeCounts.y &&
                    coord.z >= 0 && coord.z < (int)probeCounts.z) 
                {
                    uint probeIndex = GetProbeIndex(coord, probeCounts);
                    float3 probePos = volumeOrigin + coord * probeSpacing;
                    
                    float3 dir = normalize(P - probePos);
                    float dist = length(P - probePos);
                    
                    // Cosine weight
                    float weightNormal = max(dot(N, dir), 0.0f);
                    
                    // Trilinear interpolation weights
                    float3 trilinear = float3(x == 0 ? (1.0f - frac(gridPos.x)) : frac(gridPos.x),
                                              y == 0 ? (1.0f - frac(gridPos.y)) : frac(gridPos.y),
                                              z == 0 ? (1.0f - frac(gridPos.z)) : frac(gridPos.z));
                    float weightTrilinear = trilinear.x * trilinear.y * trilinear.z;
                    
                    float weight = weightNormal * weightTrilinear;
                    
                    float3 irradiance = IrradianceAtlas.SampleLevel(LinearSampler, OctEncode(N), 0).rgb;
                    accumulatedIrradiance += irradiance * weight;
                    totalWeight += weight;
                }
            }
        }
    }
    
    float3 indirectDiffuse = (totalWeight > 0.0f) ? (accumulatedIrradiance / totalWeight) : float3(0.f, 0.f, 0.f);
    return float4(indirectDiffuse, 1.0f);
}
