// ============================================================================
// ZeGFX — DXR Ray-Traced Shadows & Reflections Shader (SM 6.3)
// ============================================================================

// Raytracing payload structure for Radiance
struct RadiancePayload {
    float4 color;
    float distance;
    int bounces;
};

// Raytracing payload structure for Shadows
struct ShadowPayload {
    bool hit;
};

struct RTMaterialEntry {
    float baseColor[4];      // RGBA
    float metallic;
    float roughness;
    float emissiveStrength;
    int   alphaMode;
    float alphaCutoff;
    int   doubleSided;
    int   baseColorTextureIndex;
    int   normalTextureIndex;
};

// Global Bindings
RaytracingAccelerationStructure SceneTLAS : register(t0);
Texture2D<float> DepthBuffer : register(t1);
Texture2D<float4> NormalBuffer : register(t2);
StructuredBuffer<RTMaterialEntry> MaterialTable : register(t3);
RWTexture2D<float> OutShadowMask : register(u0);
RWTexture2D<float4> OutReflectionColor : register(u1);
SamplerState PointSampler : register(s0);

// Constant buffers
cbuffer RTConstants : register(b0) {
    float4 cameraPos;
    float4 cameraDir;
    float4 sunDirection;
    float shadowSoftness;
    float shadowMaxDistance;
    float shadowBias;
    int shadowSamples;
    float reflectionMaxDistance;
    float reflectionRoughnessCutoff;
    int reflectionBounces;
    int reflectionSamples;
    int frameIndex;
};

// Helper to reconstruct world position from UV and depth
float3 ReconstructWorldPos(float2 uv, float depth) {
    return cameraPos.xyz + cameraDir.xyz * (depth * shadowMaxDistance);
}

// Helper to decode normal from G-Buffer representation
float3 decodeNormal(float3 enc) {
    return normalize(enc * 2.0f - 1.0f);
}

// Weyl sequence pseudo-random number generator
float WeylRandom(uint2 pixelCoords, uint frame) {
    uint x = pixelCoords.x * 1973 + pixelCoords.y * 9277 + frame * 26699;
    x = (x ^ 61) ^ (x >> 16);
    x *= 9;
    x ^= x >> 4;
    x *= 0x27d4eb2d;
    x ^= x >> 15;
    return float(x) / 4294967296.0;
}

// PCG Random generator
uint pcg_hash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float3 JitterDirection(float3 dir, float softness, uint2 pixelCoords, uint sampleIdx) {
    uint seed = pcg_hash(pixelCoords.x + pixelCoords.y * 3838 + sampleIdx * 193 + frameIndex);
    float randU = float(pcg_hash(seed)) / 4294967296.0;
    float randV = float(pcg_hash(seed + 1)) / 4294967296.0;
    
    // Distribute randomly inside a cone
    float theta = randU * 2.0 * 3.14159;
    float r = sqrt(randV) * softness;
    float3 tangent = normalize(cross(dir, abs(dir.z) < 0.99 ? float3(0, 0, 1) : float3(1, 0, 0)));
    float3 bitangent = cross(dir, tangent);
    return normalize(dir + tangent * cos(theta) * r + bitangent * sin(theta) * r);
}

float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N) {
    float a = roughness * roughness;
    float phi = 2.0 * 3.14159 * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    
    return tangent * H.x + bitangent * H.y + N * H.z;
}

[shader("raygeneration")]
void RayGenShadowsReflections() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    float2 uv = (float2(launchIndex) + 0.5) / float2(launchDim);
    
    float depth = DepthBuffer.SampleLevel(PointSampler, uv, 0).r;
    if (depth >= 1.0) {
        OutShadowMask[launchIndex] = 1.0;
        OutReflectionColor[launchIndex] = float4(0, 0, 0, 0);
        return;
    }
    
    float3 worldPos = ReconstructWorldPos(uv, depth);
    float3 normal = decodeNormal(NormalBuffer.SampleLevel(PointSampler, uv, 0).xyz);
    
    // --- 1. Ray-Traced Shadow Pass ---
    float shadowOcclusion = 0.0;
    float3 shadowRayOrigin = worldPos + normal * shadowBias;
    
    for (int i = 0; i < shadowSamples; ++i) {
        float3 jitteredDir = JitterDirection(sunDirection.xyz, shadowSoftness, launchIndex, i);
        RayDesc ray;
        ray.Origin = shadowRayOrigin;
        ray.Direction = jitteredDir;
        ray.TMin = 0.02;
        ray.TMax = shadowMaxDistance;
        
        ShadowPayload payload = { true };
        TraceRay(SceneTLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                 0xFF, 0, 0, 0, ray, payload);
        shadowOcclusion += payload.hit ? 0.0 : 1.0;
    }
    OutShadowMask[launchIndex] = shadowOcclusion / float(shadowSamples);
    
    // --- 2. Ray-Traced Reflection Pass ---
    float roughness = NormalBuffer.SampleLevel(PointSampler, uv, 0).w;
    if (roughness > reflectionRoughnessCutoff) {
        OutReflectionColor[launchIndex] = float4(0, 0, 0, 0); // SSR Fallback flag
        return;
    }
    
    float3 viewDir = normalize(cameraPos.xyz - worldPos);
    float3 reflectionColorSum = 0.0;
    float totalDistance = 0.0;
    
    for (int j = 0; j < reflectionSamples; ++j) {
        float2 randVec = float2(WeylRandom(launchIndex, frameIndex * 13 + j), WeylRandom(launchIndex, frameIndex * 37 + j));
        float3 halfVector = ImportanceSampleGGX(randVec, roughness, normal);
        float3 reflectionDir = reflect(-viewDir, halfVector);
        
        RayDesc ray;
        ray.Origin = worldPos + normal * 0.02;
        ray.Direction = reflectionDir;
        ray.TMin = 0.02;
        ray.TMax = reflectionMaxDistance;
        
        RadiancePayload payload = { float4(0, 0, 0, 0), -1.0, 0 };
        TraceRay(SceneTLAS, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, payload);
        
        reflectionColorSum += payload.color.rgb;
        totalDistance += (payload.distance > 0.0) ? payload.distance : reflectionMaxDistance;
    }
    
    OutReflectionColor[launchIndex] = float4(reflectionColorSum / float(reflectionSamples), totalDistance / float(reflectionSamples));
}

[shader("miss")]
void MissShadow(inout ShadowPayload payload) {
    payload.hit = false; // No occlusion
}

[shader("miss")]
void MissReflection(inout RadiancePayload payload) {
    payload.color = float4(0.1, 0.15, 0.2, 1.0); // Ambient sky color placeholder
    payload.distance = -1.0;
}

[shader("closesthit")]
void ClosestHitReflection(inout RadiancePayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    RTMaterialEntry material = MaterialTable[InstanceContributionToHitGroupIndex()];
    
    float3 baseColor = float3(material.baseColor[0], material.baseColor[1], material.baseColor[2]);
    float3 directLight = baseColor * saturate(dot(float3(0, 1, 0), float3(0, 1, 0))); // Fake sun hit shading
    float3 emissive = baseColor * material.emissiveStrength;
    
    payload.color = float4(directLight + emissive, 1.0);
    payload.distance = RayTCurrent();
}
