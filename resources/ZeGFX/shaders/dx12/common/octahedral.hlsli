#ifndef OCTAHEDRAL_HLSLI
#define OCTAHEDRAL_HLSLI

// Encode 3D direction vector to 2D octahedral coordinates
float2 EncodeOctahedral(float3 dir) {
    float3 absDir = abs(dir);
    float L1Norm = absDir.x + absDir.y + absDir.z;
    float2 uv = dir.xy / L1Norm;
    if (dir.z < 0.0f) {
        float2 signUV = sign(uv);
        uv = (1.0f - abs(uv.yx)) * float2(signUV.x >= 0.0f ? 1.0f : -1.0f,signUV.y >= 0.0f ? 1.0f : -1.0f);
    }
    return uv * 0.5f + 0.5f;
}

// Decode 2D octahedral coordinate to 3D direction vector
float3 DecodeOctahedral(float2 uv) {
    uv = uv * 2.0f - 1.0f;
    float3 dir = float3(uv, 1.0f - (abs(uv.x) + abs(uv.y)));
    if (dir.z < 0.0f) {
        float2 signUV = sign(dir.xy);
        dir.xy = (1.0f - abs(dir.yx)) * float2(signUV.x >= 0.0f ? 1.0f : -1.0f,signUV.y >= 0.0f ? 1.0f : -1.0f);
    }
    return normalize(dir);
}

// Decode SNORM16 octahedral normal vector (in range [-1.0, 1.0])
float3 DecodeOctahedralSNORM(float2 inOct) {
    float ox = inOct.x;
    float oy = inOct.y;
    float oz = 1.0f - abs(ox) - abs(oy);

    if (oz < 0.0f) {
        float oldOx = ox;
        ox = (1.0f - abs(oy)) * (oldOx >= 0.0f ? 1.0f : -1.0f);
        oy = (1.0f - abs(oldOx)) * (oy >= 0.0f ? 1.0f : -1.0f);
    }

    return normalize(float3(ox, oy, oz));
}

// Decode SNORM16 octahedral tangent vector with handedness bit in component 0
float4 DecodeOctahedralTangentSNORM(float2 inOct) {
    int rawX = (int)round(inOct.x * 32767.0f);
    float w = ((rawX & 1) != 0) ? -1.0f : 1.0f;
    float3 t = DecodeOctahedralSNORM(inOct);
    return float4(t, w);
}

#endif
