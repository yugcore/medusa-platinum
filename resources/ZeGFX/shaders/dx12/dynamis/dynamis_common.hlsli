// ============================================================================
// Dynamis GPU Compute VFX - Common Shader Definitions & Mathematics
// Clean-Room Implementation for ZeGFX / Blueman Engine
// ============================================================================

#ifndef DYNAMIS_COMMON_HLSLI
#define DYNAMIS_COMMON_HLSLI

// ----------------------------------------------------------------------------
// Particle Structure (96 bytes, matching DynamisParticleGpu in C++)
// ----------------------------------------------------------------------------
struct DynamisParticle {
    float3 position;
    float age;              // If age < 0.0f, the particle slot is inactive/dead
    float3 velocity;
    float lifetime;
    float4 colorStart;
    float4 colorEnd;
    float sizeStart;
    float sizeEnd;
    float rotation;
    float angularVelocity;
    uint flags;
    float3 pad;
};

// ----------------------------------------------------------------------------
// Simulation Constant Buffer Layout (register b0 in compute shader)
// ----------------------------------------------------------------------------
cbuffer DynamisSimulationCB : register(b0)
{
    float3 g_EmitterPosition;
    float g_DeltaTime;
    float3 g_EmitterRotation; // Pitch, Yaw, Roll (radians)
    float g_TimeSeconds;
    float3 g_Gravity;
    float g_Drag;
    float4 g_CurlNoiseParams; // x: intensity, y: frequency, z: speed, w: reserved
    uint g_SpawnCount;
    uint g_MaxParticles;
    uint g_ShapeType;         // 0: Point, 1: Cone, 2: Sphere, 3: Box
    float g_ConeAngleRad;
    float g_InitialSpeed;
    float g_SpeedJitter;
    float g_Lifetime;
    float g_LifetimeJitter;
    float g_SizeStart;
    float g_SizeEnd;
    float g_EmitterRadius;
    float g_FloorHeight;
    float4 g_ColorStart;
    float4 g_ColorEnd;
    uint g_RandomSeed;
    uint g_CollisionFloor;
    float2 g_PadSim;
};

// ----------------------------------------------------------------------------
// Render Constant Buffer Layout (register b0 in vertex/pixel shaders)
// ----------------------------------------------------------------------------
cbuffer DynamisRenderCB : register(b0)
{
    row_major float4x4 g_ViewMatrix;
    row_major float4x4 g_ProjectionMatrix;
    float4 g_CameraPosition; // xyz, w=near
    float4 g_CameraForward;  // xyz, w=far
    float4 g_ViewportSize;   // x: width, y: height, z: 1/width, w: 1/height
    float g_SoftDepthDistance;
    uint g_BlendMode;        // 0: Additive, 1: AlphaBlend
    float2 g_PadRender;
};

// ----------------------------------------------------------------------------
// Clean-Room Pseudorandom Generator (Hash-based, non-repeating)
// ----------------------------------------------------------------------------
uint dynamis_hash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

float dynamis_random_float(inout uint state)
{
    state = dynamis_hash(state);
    return float(state & 0x00FFFFFFu) / 16777216.0f; // [0, 1)
}

float dynamis_random_range(inout uint state, float minVal, float maxVal)
{
    return minVal + dynamis_random_float(state) * (maxVal - minVal);
}

// ----------------------------------------------------------------------------
// 3D Divergence-Free Curl Noise Mathematics
// Vector field v = curl(Psi). Guaranteed div(v) == 0 (incompressible fluid motion)
// ----------------------------------------------------------------------------
float3 dynamis_potential_field(float3 p, float t, float freq)
{
    float3 coord = p * freq + float3(t * 0.35f, t * 0.42f, t * 0.28f);

    float psi_x = sin(coord.y * 1.15f + 0.72f) * cos(coord.z * 0.88f) + 0.5f * sin(coord.y * 2.2f + coord.z);
    float psi_y = sin(coord.z * 1.25f + 1.18f) * cos(coord.x * 0.82f) + 0.5f * sin(coord.z * 2.4f + coord.x);
    float psi_z = sin(coord.x * 1.35f + 2.14f) * cos(coord.y * 1.05f) + 0.5f * sin(coord.x * 2.6f + coord.y);

    return float3(psi_x, psi_y, psi_z);
}

float3 dynamis_curl_noise(float3 p, float t, float freq)
{
    const float eps = 0.035f;
    const float inv_2eps = 0.5f / eps;

    float3 dx = float3(eps, 0.0f, 0.0f);
    float3 dy = float3(0.0f, eps, 0.0f);
    float3 dz = float3(0.0f, 0.0f, eps);

    float3 psi_xp = dynamis_potential_field(p + dx, t, freq);
    float3 psi_xm = dynamis_potential_field(p - dx, t, freq);
    float3 psi_yp = dynamis_potential_field(p + dy, t, freq);
    float3 psi_ym = dynamis_potential_field(p - dy, t, freq);
    float3 psi_zp = dynamis_potential_field(p + dz, t, freq);
    float3 psi_zm = dynamis_potential_field(p - dz, t, freq);

    float dPsiZ_dy = (psi_yp.z - psi_ym.z) * inv_2eps;
    float dPsiY_dz = (psi_zp.y - psi_zm.y) * inv_2eps;

    float dPsiX_dz = (psi_zp.x - psi_zm.x) * inv_2eps;
    float dPsiZ_dx = (psi_xp.z - psi_zm.z) * inv_2eps;

    float dPsiY_dx = (psi_xp.y - psi_xm.y) * inv_2eps;
    float dPsiX_dy = (psi_yp.x - psi_ym.x) * inv_2eps;

    return float3(
        dPsiZ_dy - dPsiY_dz,
        dPsiX_dz - dPsiZ_dx,
        dPsiY_dx - dPsiX_dy
    );
}

// ----------------------------------------------------------------------------
// Rotation Vector Transformation (Euler Pitch/Yaw/Roll)
// ----------------------------------------------------------------------------
float3 dynamis_rotate_vector(float3 v, float3 eulerRad)
{
    // Pitch (X), Yaw (Y), Roll (Z)
    float cp = cos(eulerRad.x), sp = sin(eulerRad.x);
    float cy = cos(eulerRad.y), sy = sin(eulerRad.y);
    float cr = cos(eulerRad.z), sr = sin(eulerRad.z);

    // X-rotation
    float3 r1 = float3(v.x, cp * v.y - sp * v.z, sp * v.y + cp * v.z);
    // Y-rotation
    float3 r2 = float3(cy * r1.x + sy * r1.z, r1.y, -sy * r1.x + cy * r1.z);
    // Z-rotation
    return float3(cr * r2.x - sr * r2.y, sr * r2.x + cr * r2.y, r2.z);
}

#endif // DYNAMIS_COMMON_HLSLI
