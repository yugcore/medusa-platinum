// ZeGFX GPU-Driven Instance Culling Compute Shader

struct ZInstanceData {
    float4x3 WorldTransform;
    uint PrimitiveIndex;
    uint BaseVertexOffset;
    uint IndexOffset;
    uint Pad;
};

struct ZPrimitiveData {
    float3 Center;
    float Radius;
    uint MaterialID;
    uint Flags;
    uint2 Pad;
};

struct ZDrawIndexedArguments {
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int  BaseVertexLocation;
    uint StartInstanceLocation;
    uint TargetInstanceSegment;
    uint3 Pad;
};

// Bindings
StructuredBuffer<ZInstanceData> InstanceBuffer : register(t0);
StructuredBuffer<ZPrimitiveData> PrimitiveBuffer : register(t1);

RWStructuredBuffer<uint> VisibleInstanceIDs : register(u0);
RWStructuredBuffer<ZDrawIndexedArguments> IndirectArgsBuffer : register(u1);

cbuffer ZViewConstants : register(b0) {
    float4x4 ViewProjection;
    float3 CameraPosition;
    float Pad;
    float4 FrustumPlanes[6];
};

cbuffer ZCullParams : register(b1) {
    uint MaxInstances;
    uint ActiveInstanceCount;
};

bool SphereInFrustum(float3 center, float radius) {
    [unroll]
    for (uint i = 0; i < 6; ++i) {
        float4 plane = FrustumPlanes[i];
        if (dot(plane.xyz, center) + plane.w < -radius) {
            return false;
        }
    }
    return true;
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint globalInstanceId = dispatchThreadId.x;
    if (globalInstanceId >= ActiveInstanceCount || globalInstanceId >= MaxInstances) {
        return;
    }

    ZInstanceData instance = InstanceBuffer[globalInstanceId];
    ZPrimitiveData primitive = PrimitiveBuffer[instance.PrimitiveIndex];

    // Transform sphere center to world space
    float3 worldCenter = mul(float4(primitive.Center, 1.0f), instance.WorldTransform).xyz;

    // Calculate maximum scale factor from transform matrix
    float scaleX = length(instance.WorldTransform[0].xyz);
    float scaleY = length(instance.WorldTransform[1].xyz);
    float scaleZ = length(instance.WorldTransform[2].xyz);
    float maxScale = max(scaleX, max(scaleY, scaleZ));
    float worldRadius = primitive.Radius * maxScale;

    // Perform frustum culling
    if (!SphereInFrustum(worldCenter, worldRadius)) {
        return;
    }

    // Atomically increment the instance count for this draw template
    uint writeIndex = 0;
    InterlockedAdd(IndirectArgsBuffer[0].InstanceCount, 1, writeIndex);

    // Save visible instance ID in the compacted output index buffer
    uint baseOffset = IndirectArgsBuffer[0].StartInstanceLocation;
    VisibleInstanceIDs[baseOffset + writeIndex] = globalInstanceId;
}
