#ifndef ZE_SHADOW_RECEIVER_HLSLI
#define ZE_SHADOW_RECEIVER_HLSLI

// Shadow depth belongs to the rasterized face, not the interpolated lighting
// normal or its normal map. Derivatives follow the actual perspective-correct
// receiver plane; align the face sign with the authored normal for two-sided
// surfaces. Degenerate/subpixel faces retain the validated authored direction.
float3 ZeShadowGeometricReceiverNormal(float3 worldPosition, float3 authoredNormal)
{
    float3 referenceNormal = normalize(authoredNormal);
    float3 face = cross(ddx(worldPosition), ddy(worldPosition));
    float lengthSquared = dot(face, face);
    if (!all(isfinite(face)) || lengthSquared < 1e-20f)
        return referenceNormal;
    face *= rsqrt(lengthSquared);
    return dot(face, referenceNormal) < 0.0f ? -face : face;
}

// Depth at a neighbouring shadow texel must follow the receiver's geometric
// plane. Comparing every PCF tap to the centre depth makes slopes shadow
// themselves as the camera crosses texels.
float2 ZeShadowReceiverDepthGradient(float2 uv, float depth)
{
    float2 dx = ddx(uv), dy = ddy(uv);
    float determinant = dx.x * dy.y - dx.y * dy.x;
    if (!isfinite(determinant) || abs(determinant) < 1e-12f)
        return 0.0f;
    float dzdx = ddx(depth), dzdy = ddy(depth);
    float2 gradient = float2(
        dy.y * dzdx - dx.y * dzdy,
        dx.x * dzdy - dy.x * dzdx) / determinant;
    return all(isfinite(gradient)) ? clamp(gradient, -4096.0f, 4096.0f) : 0.0f;
}

float ZeShadowReceiverTapDepth(float depth, float2 gradient, float2 deltaUv)
{
    // Limit discontinuities at silhouettes and degenerate projected planes.
    return depth + clamp(dot(gradient, deltaUv), -0.02f, 0.02f);
}

// Directional shadow projections are orthographic. Solve the receiver plane
// from their linear basis instead of differencing translated shadow positions.
// At kilometre-scale coordinates ddx(depth)/ddx(uv) amplifies float rounding
// into visible, frame-dependent self-shadow speckles on otherwise flat faces.
float2 ZeShadowPlaneDepthGradient(float3 normal, float4x4 projection, float2 atlasScale)
{
    float3 x = float3(projection[0][0],projection[1][0],projection[2][0]);
    float3 y = float3(projection[0][1],projection[1][1],projection[2][1]);
    float3 z = float3(projection[0][2],projection[1][2],projection[2][2]);
    float3 plane = float3(dot(normal,x)/max(dot(x,x),1e-12f),
                          dot(normal,y)/max(dot(y,y),1e-12f),
                          dot(normal,z)/max(dot(z,z),1e-12f));
    if(abs(plane.z)<1e-5f)return 0;
    float2 gradient=float2(-2*plane.x,2*plane.y)/(plane.z*max(atlasScale,1e-6f));
    return all(isfinite(gradient)) ? clamp(gradient,-4096.0f,4096.0f) : 0;
}
#endif
