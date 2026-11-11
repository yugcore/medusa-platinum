#ifndef ZE_GI_COMMON_HLSLI
#define ZE_GI_COMMON_HLSLI

// 3D grid coord to linear index remapping
uint GetProbeIndex(uint3 coord, uint3 probeCounts) {
    return coord.x + probeCounts.x * (coord.y + probeCounts.y * coord.z);
}

// Convert direction to octahedral projection UV coordinates
float2 OctEncode(float3 dir) {
    float3 l1norm = dir / (abs(dir.x) + abs(dir.y) + abs(dir.z));
    float2 uv = l1norm.xy;
    if (l1norm.z < 0.0f) {
        uv = (1.0f - abs(uv.yx)) * (uv >= 0.0f ? 1.0f : -1.0f);
    }
    return uv * 0.5f + 0.5f;
}

// Convert octahedral UV coordinate to direction
float3 OctDecode(float2 uv) {
    uv = uv * 2.0f - 1.0f;
    float3 dir = float3(uv.x, uv.y, 1.0f - abs(uv.x) - abs(uv.y));
    if (dir.z < 0.0f) {
        float2 signVal = float2(dir.x >= 0.0f ? 1.0f : -1.0f, dir.y >= 0.0f ? 1.0f : -1.0f);
        dir.xy = (1.0f - abs(dir.yx)) * signVal;
    }
    return normalize(dir);
}

#endif // ZE_GI_COMMON_HLSLI
