struct InstanceShaderData {
    float4x4 WorldTransform;
    float4x4 PrevWorldTransform;
    float4   BoundsMin;
    float4   BoundsMax;
    uint     MaterialIndex;
    uint     Flags;
    uint     Unused0;
    uint     Unused1;
};

StructuredBuffer<InstanceShaderData> InstanceBuffer : register(t0);

InstanceShaderData GetInstanceData(uint InstanceID) {
    return InstanceBuffer[InstanceID];
}

// Chapter 5.1 layout for FPrimitiveBuffer
struct FPrimitiveData
{
    float4x3 LocalToWorld;
    float4x3 WorldToLocal;
    float4 SphereBounds; // center, radius
    uint MaterialIndex;
    uint Flags;
};

// Chapter 5.1 layout for FInstanceBuffer
struct FInstanceData
{
    float4x3 LocalToWorld;
    uint ParentPrimitiveIndex;
    float RandomValue;
    float2 CustomColor;
    uint CustomDataOffset;
    uint CustomDataFlags;
};

StructuredBuffer<FPrimitiveData> PrimitiveBuffer : register(t1);
StructuredBuffer<FInstanceData> FInstanceBuffer : register(t2);
StructuredBuffer<uint> VisibleInstanceIds : register(t3);

