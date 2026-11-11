#include "../common/octahedral.hlsli"

struct ProbeData {
    float4 Irradiance;
    float4 Distance;
};

RWStructuredBuffer<ProbeData> ProbeBuffer : register(u0);
Texture2D<float4> RadianceCacheAtlas : register(t0);

cbuffer GiVolumeParams : register(b0) {
    float4 GridCenter;
    float4 GridSpacing;
    uint3  GridDimensions;
    uint   TotalProbes;
};

[numthreads(64, 1, 1)]
void UpdateProbesCS(uint3 DispatchThreadID : SV_DispatchThreadID) {
    uint ProbeIndex = DispatchThreadID.x;
    if (ProbeIndex >= TotalProbes) return;

    float4 currentIrradiance = ProbeBuffer[ProbeIndex].Irradiance;
    float4 sampleRadiance = RadianceCacheAtlas.Load(int3(ProbeIndex % 512, ProbeIndex / 512, 0));

    float hysteresis = 0.95f;
    ProbeBuffer[ProbeIndex].Irradiance = currentIrradiance * hysteresis + sampleRadiance * (1.0f - hysteresis);
}
