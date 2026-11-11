#ifndef ZE_RAY_COUNTERS_HLSLI
#define ZE_RAY_COUNTERS_HLSLI
// Category is uniform for a dispatch/draw. Aggregate the currently active wave,
// including divergent ray paths, before updating the shared GPU counters.
void ZeCountShadingRay(RWByteAddressBuffer counters,uint category,bool hit) {
    uint rays=WaveActiveCountBits(true),hits=WaveActiveCountBits(hit);
    if(WaveIsFirstLane()) {
        uint ignored;
        counters.InterlockedAdd(category*16,rays,ignored);
        counters.InterlockedAdd(category*16+4,hits,ignored);
        counters.InterlockedAdd(category*16+8,rays-hits,ignored);
    }
}
#endif
