#include "../common/ze_light_types.hlsli"

Texture2D<float> depthTex : register(t0);
StructuredBuffer<GpuLocalLight> localLights : register(t1);

RWStructuredBuffer<uint> tileLightIndexBuffer : register(u0);
RWStructuredBuffer<uint2> tileLightOffsetCountBuffer : register(u1); // x: offset, y: count
RWStructuredBuffer<uint> overflowCounter : register(u2); // element 0: global index allocation offset, element 1: overflow flag

cbuffer CameraParams : register(b0, space0) {
    row_major float4x4 gProj;
    row_major float4x4 gInvProj;
    row_major float4x4 gView;
    float2 gViewportSize;
    uint gNumTilesX;
    uint gNumTilesY;
    uint gNumLocalLights;
    float gNearPlane;
    float gFarPlane;
};

groupshared uint gsMinDepthInt;
groupshared uint gsMaxDepthInt;
groupshared uint gsLightCount;
groupshared uint gsLightIndices[256];
groupshared uint gsGlobalOffset;

[numthreads(16, 16, 1)]
void CSMain(uint3 groupID : SV_GroupID,
            uint3 groupThreadID : SV_GroupThreadID,
            uint groupIndex : SV_GroupIndex)
{
    // Initialize shared variables
    if (groupIndex == 0) {
        gsMinDepthInt = 0xFFFFFFFF;
        gsMaxDepthInt = 0;
        gsLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    
    // Calculate pixel coordinates
    uint2 pixelPos = groupID.xy * 16 + groupThreadID.xy;
    float depth = 0.0f;
    if (pixelPos.x < (uint)gViewportSize.x && pixelPos.y < (uint)gViewportSize.y) {
        depth = depthTex.Load(uint3(pixelPos, 0));
    }
    
    // Reduce depth to find min/max
    uint depthInt = asuint(depth);
    if (depth > 0.0f && depth < 1.0f) {
        InterlockedMin(gsMinDepthInt, depthInt);
        InterlockedMax(gsMaxDepthInt, depthInt);
    }
    GroupMemoryBarrierWithGroupSync();
    
    // Construct tile frustum planes in view space
    // We will do a simplified AABB culling in view space for absolute correctness and speed
    float4 frustumPlanes[4];
    float2 tileScale = gViewportSize / 16.0f;
    float2 tileBias = tileScale - 2.0f * groupID.xy;
    
    // left, right, bottom, top planes
    frustumPlanes[0] = float4(gProj[0][0] * tileScale.x, 0.0f, tileBias.x - 1.0f, 0.0f);
    frustumPlanes[1] = float4(-gProj[0][0] * tileScale.x, 0.0f, -tileBias.x - 1.0f, 0.0f);
    frustumPlanes[2] = float4(0.0f, gProj[1][1] * tileScale.y, tileBias.y - 1.0f, 0.0f);
    frustumPlanes[3] = float4(0.0f, -gProj[1][1] * tileScale.y, -tileBias.y - 1.0f, 0.0f);
    
    for (uint i = 0; i < 4; ++i) {
        frustumPlanes[i] /= length(frustumPlanes[i].xyz);
    }
    
    // Convert min/max depth bounds to view space Z
    float minZ = gNearPlane;
    float maxZ = gFarPlane;
    
    if (gsMinDepthInt != 0xFFFFFFFF && gsMaxDepthInt != 0) {
        float minDepth = asfloat(gsMinDepthInt);
        float maxDepth = asfloat(gsMaxDepthInt);
        
        float4 clipMin = float4(0.0f, 0.0f, minDepth, 1.0f);
        float4 viewMin = mul(clipMin, gInvProj);
        viewMin /= viewMin.w;
        
        float4 clipMax = float4(0.0f, 0.0f, maxDepth, 1.0f);
        float4 viewMax = mul(clipMax, gInvProj);
        viewMax /= viewMax.w;
        
        minZ = min(viewMin.z, viewMax.z);
        maxZ = max(viewMin.z, viewMax.z);
        
        // Clamp bounds to near/far plane for safety
        minZ = max(minZ, gNearPlane);
        maxZ = min(maxZ, gFarPlane);
    }
    
    // Cull lights
    for (uint lightIdx = groupIndex; lightIdx < gNumLocalLights; lightIdx += 256) {
        GpuLocalLight light = localLights[lightIdx];
        float3 worldPos = light.positionRange.xyz;
        float range = light.positionRange.w;
        
        // Transform light position to view space (using row-major matrix convention)
        float3 viewPos = mul(float4(worldPos, 1.0f), gView).xyz;
        
        // Check depth bounds
        if (viewPos.z + range < minZ || viewPos.z - range > maxZ) {
            continue;
        }
        
        // Check frustum planes
        bool inFrustum = true;
        for (uint p = 0; p < 4; ++p) {
            if (dot(frustumPlanes[p].xyz, viewPos) + range < 0.0f) {
                inFrustum = false;
                break;
            }
        }
        
        if (inFrustum) {
            uint slot;
            InterlockedAdd(gsLightCount, 1, slot);
            if (slot < 256) {
                gsLightIndices[slot] = lightIdx;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();
    
    // Allocate global storage index
    if (groupIndex == 0) {
        uint count = min(gsLightCount, 256u);
        if (gsLightCount > 256) {
            InterlockedOr(overflowCounter[1], 1u); // set overflow flag
        }
        InterlockedAdd(overflowCounter[0], count, gsGlobalOffset);
        
        uint tileIdx = groupID.y * gNumTilesX + groupID.x;
        tileLightOffsetCountBuffer[tileIdx] = uint2(gsGlobalOffset, count);
    }
    GroupMemoryBarrierWithGroupSync();
    
    // Copy shared light indices to global buffer
    uint count = min(gsLightCount, 256u);
    for (uint idx = groupIndex; idx < count; idx += 256) {
        tileLightIndexBuffer[gsGlobalOffset + idx] = gsLightIndices[idx];
    }
}
