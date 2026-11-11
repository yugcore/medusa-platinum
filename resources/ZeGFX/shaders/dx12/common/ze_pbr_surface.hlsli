// ============================================================================
// ZeGFX — Shared PBR Surface Sampling & Tangent-Space Helpers
// ============================================================================
#ifndef ZE_PBR_SURFACE_HLSLI
#define ZE_PBR_SURFACE_HLSLI

float3 ZeUnitVector(float3 v, float3 fallback) {
    float lengthSquared = dot(v, v);
    return lengthSquared > 1e-20f ? v * rsqrt(max(lengthSquared, 1e-20f)) : fallback;
}

// Row-vector convention: the rows of M are the transformed basis vectors.
// Cofactors give inverse-transpose direction without a matrix inverse/divide.
// Preserve determinant sign so mirrored objects keep the authored normal side.
float3 ZeTransformNormal(float3 normal, float3x3 transform) {
    float3 c0 = cross(transform[1], transform[2]);
    float3 c1 = cross(transform[2], transform[0]);
    float3 c2 = cross(transform[0], transform[1]);
    float orientation = dot(transform[0], c0) < 0.0f ? -1.0f : 1.0f;
    float3 transformed = (normal.x * c0 + normal.y * c1 + normal.z * c2) * orientation;
    return ZeUnitVector(transformed, ZeUnitVector(normal, float3(0, 1, 0)));
}

// Geometric roughness filtering (Toksvig / Kaplanyan clean shading).
// Sub-pixel normal variance across screen pixel footprint expands the microfacet
// roughness distribution, preventing high-frequency specular aliasing and fireflies
// on distant geometry without washing out authored close-up highlights.
float ZeFilterRoughness(float roughness, float3 normalDx, float3 normalDy) {
    float r = saturate(roughness);
    float a2 = r * r * r * r;
    float variance = 0.5f * (dot(normalDx, normalDx) + dot(normalDy, normalDy));
    float filteredA2 = saturate(a2 + 0.35f * variance);
    return sqrt(sqrt(filteredA2));
}

// Decodes tangent-space normal from sample (supporting BC5 two-channel normal as well as RGB/RGBA)
// and transforms it to world space using an orthogonalized TBN matrix with handedness preservation.
float3 DecodeWorldNormalFromMap(
    float4 normalSample,
    float3 worldNormal,
    float3 worldTangent,
    float3 worldBitan
) {
    float2 normalXY = normalSample.xy * 2.0f - 1.0f;
    float3 tsNormal = (normalSample.z < 0.001f)
        ? float3(normalXY, sqrt(saturate(1.0f - dot(normalXY, normalXY))))
        : normalSample.xyz * 2.0f - 1.0f;

    // Gram-Schmidt re-orthogonalize T against N
    float3 Nv = ZeUnitVector(worldNormal, float3(0, 1, 0));
    float3 axis = abs(Nv.z) < 0.9f ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 fallbackT = normalize(cross(axis, Nv));
    float3 T = ZeUnitVector(worldTangent - dot(worldTangent, Nv) * Nv, fallbackT);

    // Reconstruct B while preserving the tangent handedness
    float3 B = worldBitan;
    float tangentHandedness = dot(cross(Nv, T), B) < 0.0f ? -1.0f : 1.0f;
    B = cross(Nv, T) * tangentHandedness;

    // TBN transforms tangent-space normal to world space
    float3x3 TBN = float3x3(T, B, Nv);
    return ZeUnitVector(mul(tsNormal, TBN), Nv);
}

#endif // ZE_PBR_SURFACE_HLSLI
