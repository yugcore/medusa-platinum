// ============================================================================
// ZeGFX — Native HLSL 6.5 Mesh Shader Kernel
// ============================================================================

struct MeshVertexOutput {
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct MeshletTriangle {
    uint v0;
    uint v1;
    uint v2;
};

cbuffer MeshShaderConstants : register(b0) {
    float4x4 ViewProjection;
};

[outputtopology("triangle")]
[numthreads(64, 1, 1)]
void MSMain(
    uint gtid : SV_GroupIndex,
    uint gid : SV_GroupID,
    out vertices MeshVertexOutput verts[64],
    out indices uint3 triangles[32]
) {
    // Specify number of output vertices and primitives for this meshlet
    SetMeshOutputCounts(64, 32);

    if (gtid < 64) {
        float x = (float)(gtid % 8) - 4.0f;
        float y = (float)(gtid / 8) - 4.0f;
        float3 pos = float3(x, y, 0.0f);

        verts[gtid].position = mul(ViewProjection, float4(pos, 1.0f));
        verts[gtid].normal = float3(0.0f, 0.0f, 1.0f);
        verts[gtid].uv = float2((x + 4.0f) / 8.0f, (y + 4.0f) / 8.0f);
    }

    if (gtid < 32) {
        uint base = gtid * 2;
        triangles[gtid] = uint3(base, base + 1, base + 2);
    }
}
