// ZeGFX Bindless Mesh Submission Shader (HLSL Shader Model 6.6)

struct ZVertex {
    float3 Position;
    float3 Normal;
    float2 UV;
    float4 Tangent;
};

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

struct ZMaterialData {
    uint AlbedoMapIndex;
    uint NormalMapIndex;
    uint RoughnessMetallicMapIndex;
    float4 BaseColorTint;
};

// Global persistent structured buffers
StructuredBuffer<ZVertex> GlobalVertexBuffer : register(t0);
StructuredBuffer<uint> GlobalIndexBuffer : register(t1);
StructuredBuffer<ZInstanceData> InstanceBuffer : register(t2);
StructuredBuffer<ZPrimitiveData> PrimitiveBuffer : register(t3);
StructuredBuffer<uint> VisibleInstanceIDs : register(t4);
StructuredBuffer<ZMaterialData> GlobalMaterialBuffer : register(t5);

// Global view constants pushed via Root Signature
cbuffer ZViewConstants : register(b0) {
    float4x4 ViewProjection;
    float3 CameraPosition;
    float Pad;
};

struct VSInput {
    uint VertexID : SV_VertexID;
    uint DrawInstanceID : SV_InstanceID; // Compacted instance index from culling
};

struct VSOutput {
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD0;
    float2 UV : TEXCOORD1;
    nointerpolation uint MaterialID : TEXCOORD3;
};

VSOutput MainVS(VSInput Input) {
    VSOutput Output;

    // Resolve global instance index
    uint GlobalInstanceID = VisibleInstanceIDs[Input.DrawInstanceID];
    ZInstanceData instance = InstanceBuffer[GlobalInstanceID];
    ZPrimitiveData primitive = PrimitiveBuffer[instance.PrimitiveIndex];

    // Fetch geometry offsets from the instance state
    uint BaseVertex = instance.BaseVertexOffset;
    uint IndexOffset = instance.IndexOffset;

    // Load vertex index and position
    uint Index = GlobalIndexBuffer[IndexOffset + Input.VertexID];
    ZVertex vertex = GlobalVertexBuffer[BaseVertex + Index];

    // Transform position to world then clip space
    float3 worldPos = mul(float4(vertex.Position, 1.0f), instance.WorldTransform).xyz;
    Output.Position = mul(float4(worldPos, 1.0f), ViewProjection);

    // Transform normal
    Output.Normal = mul(vertex.Normal, (float3x3)instance.WorldTransform);
    Output.UV = vertex.UV;
    Output.MaterialID = primitive.MaterialID;

    return Output;
}

float4 MainPS(VSOutput Input) : SV_Target {
    // Fetch material data by index
    ZMaterialData material = GlobalMaterialBuffer[Input.MaterialID];

    // Sample texture dynamically from Global Resource Descriptor Heap (Shader Model 6.6 bindless)
#if defined(__SHADER_STAGE_PIXEL__) && (__SHADER_MODEL_MAJOR__ >= 6 && __SHADER_MODEL_MINOR__ >= 6)
    Texture2D albedoTex = ResourceDescriptorHeap[material.AlbedoMapIndex];
    SamplerState linearSampler = SamplerDescriptorHeap[0];
    float4 color = albedoTex.Sample(linearSampler, Input.UV) * material.BaseColorTint;
#else
    // Fallback if SM 6.6 descriptor heap is not active during compile/platform target
    float4 color = material.BaseColorTint;
#endif

    return color;
}
