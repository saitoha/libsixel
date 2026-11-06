#include <metal_stdlib>
using namespace metal;

/* One 32-lane group owns an 8x8 tile, two queries per lane. The closed
 * integer box encloses every active fractional query. Its distance bounds
 * are exact integers; final distances use the original fractional values.
 * No residual, source image or retained-history buffer is visible here. */
kernel void blockcull_fractional(
    constant uchar *palette [[buffer(0)]],
    constant uint &count [[buffer(1)]],
    device const float4 *queries [[buffer(4)]],
    device uint *indexes [[buffer(5)]],
    constant uint2 &extent [[buffer(6)]],
    uint2 tile [[threadgroup_position_in_grid]],
    uint lane [[thread_index_in_threadgroup]])
{
    threadgroup uchar candidates[64];
    float4 q[2];
    uint at[2];
    bool inside[2];
    uint2 xy;
    uint3 low, high;
    int3 color, gap;
    uint h, i, base, slot, keep, kept, bound, near, far, index, best;
    float minimum, distance;
    float3 difference;

    low = uint3(255);
    high = uint3(0);
    for (h = 0; h < 2; ++h) {
        i = lane+h*32;
        xy = tile*8+uint2(i%8, i/8);
        inside[h] = all(xy < extent);
        at[h] = inside[h] ? xy.y*extent.x+xy.x : 0;
        q[h] = inside[h] ? queries[at[h]] : float4(0.f);
        if (q[h].w != 0.f) {
            low = min(low, uint3(floor(q[h].xyz)));
            high = max(high, uint3(ceil(q[h].xyz)));
        }
        if (inside[h]) indexes[at[h]] = count;
    }
    if (simd_all(q[0].w == 0.f && q[1].w == 0.f)) return;
    low = uint3(simd_min(low.x), simd_min(low.y), simd_min(low.z));
    high = uint3(simd_max(high.x), simd_max(high.y), simd_max(high.z));
    bound = UINT_MAX;
    for (i = lane; i < count; i += 32) {
        color = int3(palette[3*i], palette[3*i+1], palette[3*i+2]);
        gap = max(color-int3(low), int3(high)-color);
        far = uint(gap.x*gap.x+gap.y*gap.y+gap.z*gap.z);
        bound = min(bound, far);
    }
    bound = simd_min(bound);
    kept = 0;
    for (base = 0; base < count && kept <= 64; base += 32) {
        i = base+lane;
        keep = 0;
        if (i < count) {
            color = int3(palette[3*i], palette[3*i+1], palette[3*i+2]);
            gap = max(max(int3(low)-color, color-int3(high)), int3(0));
            near = uint(gap.x*gap.x+gap.y*gap.y+gap.z*gap.z);
            keep = near <= bound ? 1u : 0u;
        }
        slot = kept+simd_prefix_exclusive_sum(keep);
        if (keep && slot < 64) candidates[slot] = uchar(i);
        kept += simd_sum(keep);
    }
    simdgroup_barrier(mem_flags::mem_threadgroup);
    for (h = 0; h < 2; ++h) {
        if (!inside[h] || q[h].w == 0.f) continue;
        best = 0;
        minimum = INFINITY;
        /* Overflow scans the whole palette, never a truncated list. */
        for (i = 0; i < (kept > 64 ? count : kept); ++i) {
            index = kept > 64 ? i : uint(candidates[i]);
            difference = q[h].xyz - float3(palette[3*index],
                palette[3*index+1], palette[3*index+2]);
            distance = dot(difference, difference);
            if (distance < minimum) { minimum = distance; best = index; }
        }
        indexes[at[h]] = best;
    }
}
