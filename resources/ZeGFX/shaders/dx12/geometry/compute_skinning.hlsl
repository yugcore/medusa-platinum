// ============================================================================
// ZeGFX — GPU Compute Shader Skeletal Skinning Kernel
// ============================================================================

struct SkinnedVertexInput {
    float3 position;
    float3 normal;
    float2 uv;
    uint4  boneIndices;
    float4 boneWeights;
};

struct SkinnedVertexOutput {
    float3 position;
    float3 normal;
    float2 uv;
};

cbuffer BonePaletteCB : register(b0) {
    float4x4 BoneTransforms[128];
    uint     VertexCount;
};

StructuredBuffer<SkinnedVertexInput> SourceVertices : register(t0);
RWStructuredBuffer<SkinnedVertexOutput> DeformedVertices : register(u0);

[numthreads(64, 1, 1)]
void CS_SkinVertices(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint vertexIdx = dispatchThreadId.x;
    if (vertexIdx >= VertexCount) return;

    SkinnedVertexInput inVert = SourceVertices[vertexIdx];

    // Blend bone matrices
    float4x4 skinTransform =
        inVert.boneWeights.x * BoneTransforms[inVert.boneIndices.x] +
        inVert.boneWeights.y * BoneTransforms[inVert.boneIndices.y] +
        inVert.boneWeights.z * BoneTransforms[inVert.boneIndices.z] +
        inVert.boneWeights.w * BoneTransforms[inVert.boneIndices.w];

    float4 skinnedPos = mul(skinTransform, float4(inVert.position, 1.0f));
    float3 skinnedNormal = mul((float3x3)skinTransform, inVert.normal);

    SkinnedVertexOutput outVert;
    outVert.position = skinnedPos.xyz;
    outVert.normal = normalize(skinnedNormal);
    outVert.uv = inVert.uv;

    DeformedVertices[vertexIdx] = outVert;
}
