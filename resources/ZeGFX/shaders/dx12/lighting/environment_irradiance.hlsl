#include "../common/ze_binding_spaces.hlsli"

struct PSInput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

TextureCube skyMap : register(t0);
SamplerState linearSampler : register(s0);

float4 PSMain(PSInput input) : SV_Target {
    float3 N = normalize(input.normal);
    float3 irradiance = float3(0.0f, 0.0f, 0.0f);
    
    // Riemann sum convolution over the hemisphere
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    
    float sampleDelta = 0.05f;
    float numSamples = 0.0f;
    for (float phi = 0.0f; phi < 2.0f * 3.14159265f; phi += sampleDelta) {
        for (float theta = 0.0f; theta < 0.5f * 3.14159265f; theta += sampleDelta) {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N; 
            
            irradiance += skyMap.SampleLevel(linearSampler, sampleVec, 0).rgb * cos(theta) * sin(theta);
            numSamples++;
        }
    }
    irradiance = 3.14159265f * irradiance * (1.0f / numSamples);
    return float4(irradiance, 1.0f);
}
