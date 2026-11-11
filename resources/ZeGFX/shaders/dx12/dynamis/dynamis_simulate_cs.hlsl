// ============================================================================
// Dynamis GPU Compute VFX - Particle Simulation Compute Shader
// Clean-Room Implementation for ZeGFX / Blueman Engine
// ============================================================================

#include "dynamis_common.hlsli"

RWStructuredBuffer<DynamisParticle> g_Particles : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint idx = dispatchThreadID.x;
    if (idx >= g_MaxParticles)
        return;

    DynamisParticle p = g_Particles[idx];

    // ========================================================================
    // Dead / Inactive Slot: Check if this thread can spawn a new particle
    // ========================================================================
    if (p.age < 0.0f)
    {
        if (idx < g_SpawnCount)
        {
            uint rng = dynamis_hash(idx ^ g_RandomSeed ^ (asuint(g_TimeSeconds) * 1973u));

            p.age = 0.0f;
            p.lifetime = max(0.05f, g_Lifetime * (1.0f + dynamis_random_range(rng, -g_LifetimeJitter, g_LifetimeJitter)));
            p.colorStart = g_ColorStart;
            p.colorEnd = g_ColorEnd;
            p.sizeStart = g_SizeStart;
            p.sizeEnd = g_SizeEnd;
            p.rotation = dynamis_random_range(rng, 0.0f, 6.2831853f);
            p.angularVelocity = dynamis_random_range(rng, -3.14159f, 3.14159f);
            p.flags = 0u;

            float speed = g_InitialSpeed * max(0.05f, 1.0f + dynamis_random_range(rng, -g_SpeedJitter, g_SpeedJitter));

            if (g_ShapeType == 1u) // Cone
            {
                float diskR = sqrt(dynamis_random_float(rng)) * g_EmitterRadius;
                float diskPhi = dynamis_random_range(rng, 0.0f, 6.2831853f);
                float3 offset = float3(diskR * cos(diskPhi), 0.0f, diskR * sin(diskPhi));

                float theta = dynamis_random_range(rng, 0.0f, g_ConeAngleRad);
                float phi = dynamis_random_range(rng, 0.0f, 6.2831853f);
                float sinT = sin(theta);
                float3 localDir = float3(sinT * cos(phi), cos(theta), sinT * sin(phi));

                float3 worldOffset = dynamis_rotate_vector(offset, g_EmitterRotation);
                float3 worldDir = dynamis_rotate_vector(localDir, g_EmitterRotation);

                p.position = g_EmitterPosition + worldOffset;
                p.velocity = normalize(worldDir) * speed;
            }
            else if (g_ShapeType == 2u) // Sphere
            {
                float u = dynamis_random_range(rng, -1.0f, 1.0f);
                float phi = dynamis_random_range(rng, 0.0f, 6.2831853f);
                float r = sqrt(max(0.0f, 1.0f - u * u));
                float3 dir = float3(r * cos(phi), u, r * sin(phi));

                p.position = g_EmitterPosition + dir * dynamis_random_range(rng, 0.0f, g_EmitterRadius);
                p.velocity = dir * speed;
            }
            else if (g_ShapeType == 3u) // Box
            {
                float3 boxOffset = float3(
                    dynamis_random_range(rng, -1.0f, 1.0f),
                    dynamis_random_range(rng, -1.0f, 1.0f),
                    dynamis_random_range(rng, -1.0f, 1.0f)
                ) * g_EmitterRadius;

                p.position = g_EmitterPosition + boxOffset;
                p.velocity = float3(
                    dynamis_random_range(rng, -0.3f, 0.3f),
                    speed,
                    dynamis_random_range(rng, -0.3f, 0.3f)
                );
            }
            else // Point
            {
                float theta = dynamis_random_range(rng, 0.0f, g_ConeAngleRad);
                float phi = dynamis_random_range(rng, 0.0f, 6.2831853f);
                float sinT = sin(theta);
                float3 localDir = float3(sinT * cos(phi), cos(theta), sinT * sin(phi));
                float3 worldDir = dynamis_rotate_vector(localDir, g_EmitterRotation);

                p.position = g_EmitterPosition;
                p.velocity = normalize(worldDir) * speed;
            }

            g_Particles[idx] = p;
        }
        return;
    }

    // ========================================================================
    // Alive Particle: Physics, Curl Noise Turbulence, and Integration
    // ========================================================================
    p.age += g_DeltaTime;
    if (p.age >= p.lifetime)
    {
        p.age = -1.0f; // Slot liberated for future reuse
        g_Particles[idx] = p;
        return;
    }

    // Apply linear air drag / damping
    float dragFactor = max(0.0f, 1.0f - g_Drag * g_DeltaTime);
    p.velocity *= dragFactor;

    // Evaluate analytical 3D curl noise turbulence (divergence-free flow)
    float3 curl = dynamis_curl_noise(
        p.position,
        g_TimeSeconds * g_CurlNoiseParams.z,
        g_CurlNoiseParams.y
    );
    float3 curlForce = curl * g_CurlNoiseParams.x;

    // Apply accelerations
    p.velocity += (g_Gravity + curlForce) * g_DeltaTime;

    // Advance position
    p.position += p.velocity * g_DeltaTime;

    // Floor collision & bounce (if enabled)
    if (g_CollisionFloor != 0u && p.position.y <= g_FloorHeight)
    {
        p.position.y = g_FloorHeight;
        p.velocity.y = abs(p.velocity.y) * 0.35f;
        p.velocity.xz *= 0.75f;
    }

    // Advance angular rotation
    p.rotation += p.angularVelocity * g_DeltaTime;

    g_Particles[idx] = p;
}
