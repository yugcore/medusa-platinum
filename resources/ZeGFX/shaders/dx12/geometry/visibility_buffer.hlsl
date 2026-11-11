// ============================================================================
// ZeGFX — Compact 64-bit Visibility Buffer Architecture (InstanceID + PrimitiveID)
// ============================================================================

struct VisibilityBufferPayload {
    uint depthUint;
    uint instancePrimitivePayload; // Upper 16 bits = InstanceID, Lower 16 bits = PrimitiveID
};

cbuffer VisibilityBufferConstants : register(b0) {
    float4x4 ViewProjection;
    uint2    ViewportDimensions;
};

RWTexture2D<uint2> VisibilityTarget : register(u0); // Compact 64-bit payload

uint PackVisibilityPayload(uint instanceId, uint primitiveId) {
    return ((instanceId & 0xFFFF) << 16) | (primitiveId & 0xFFFF);
}

void UnpackVisibilityPayload(uint packed, out uint instanceId, out uint primitiveId) {
    instanceId  = (packed >> 16) & 0xFFFF;
    primitiveId = packed & 0xFFFF;
}

[numthreads(16, 16, 1)]
void CS_ClearVisibilityBuffer(uint3 dispatchThreadId : SV_DispatchThreadID) {
    if (dispatchThreadId.x >= ViewportDimensions.x || dispatchThreadId.y >= ViewportDimensions.y) return;
    
    // Clear depth to 0 (Reverse-Z) and payload to 0xFFFFFFFF
    VisibilityTarget[dispatchThreadId.xy] = uint2(0, 0xFFFFFFFF);
}
