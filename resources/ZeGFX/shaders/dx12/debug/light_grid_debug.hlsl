Texture2D<float4> sourceTex : register(t0);
StructuredBuffer<uint2> tileLightOffsetCountBuffer : register(t1);
SamplerState pointSampler : register(s0);

cbuffer CameraParams : register(b0, space0) {
    uint gNumTilesX;
    uint gNumTilesY;
    float2 gViewportSize;
    uint gTileSize;
    uint gMaxLightsPerTile;
    uint gLightGridEnabled;
    uint gPadding;
};

struct PSInput {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

float4 PSMain(PSInput input) : SV_Target {
    float4 color = sourceTex.SampleLevel(pointSampler, input.uv, 0);
    
    if (gLightGridEnabled == 0) {
        return color;
    }

    uint2 pixelPos = uint2(input.pos.xy);
    uint2 tileId = pixelPos / max(gTileSize, 1u);
    
    if (tileId.x >= gNumTilesX || tileId.y >= gNumTilesY) {
        return color;
    }
    
    uint tileIdx = tileId.y * gNumTilesX + tileId.x;
    uint2 offsetCount = tileLightOffsetCountBuffer[tileIdx];
    uint count = min(offsetCount.y, gMaxLightsPerTile);
    
    float3 debugColor = float3(0.0f, 0.0f, 0.0f);
    if (count > 0) {
        if (count >= gMaxLightsPerTile) {
            debugColor = float3(1.0f, 0.0f, 1.0f); // Magenta for overflow
        } else {
            float factor = saturate((float)count / 15.0f);
            debugColor = lerp(float3(0.0f, 1.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), factor);
        }
        return float4(lerp(color.rgb, debugColor, 0.5f), 1.0f);
    }
    
    return color;
}
