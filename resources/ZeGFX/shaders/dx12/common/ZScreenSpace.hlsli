// ZScreenSpace.hlsli - Shared Screen Space Transformation Header

struct ZViewportTransform
{
    float2 SourceScale;
    float2 SourceBias;
    float2 DestScale;
    float2 DestBias;
    uint   InputTextureIndex;
    uint   InputSamplerIndex;
    uint   Padding0;
    uint   Padding1;
};

// Computes input UV coordinates from current pixel position (SV_Position)
float2 MapScreenCoords(float2 svPosition, ZViewportTransform xform)
{
    // Convert SV_Position pixel coordinates to normalized viewport UVs [0, 1]
    float2 viewportUV = svPosition.xy * xform.DestScale + xform.DestBias;
    
    // Transform viewport UVs to source texture coordinate space
    return viewportUV * xform.SourceScale + xform.SourceBias;
}
