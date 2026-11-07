#include <metal_stdlib>
using namespace metal;

/* One 32-thread group owns an 8x8 tile, two queries per lane. The closed
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
    uint lane [[thread_index_in_threadgroup]],
    uint simd_width [[threads_per_simdgroup]])
{
    threadgroup uchar candidates[64];
    threadgroup uint3 lows[32], highs[32];
    threadgroup uint bounds[32], prefix[33];
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
    /* A fixed threadgroup may contain several hardware SIMD groups.
     * Preserve the single-SIMD fast path, but reduce the whole tile when
     * the device executes narrower groups (including Intel GPUs). */
    if (simd_width == 32u) {
        low = uint3(simd_min(low.x), simd_min(low.y), simd_min(low.z));
        high = uint3(simd_max(high.x), simd_max(high.y), simd_max(high.z));
    } else {
        lows[lane] = low;
        highs[lane] = high;
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (lane == 0) {
            for (i = 1; i < 32; ++i) {
                lows[0] = min(lows[0], lows[i]);
                highs[0] = max(highs[0], highs[i]);
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        low = lows[0];
        high = highs[0];
    }
    if (any(low > high)) return;
    bound = UINT_MAX;
    for (i = lane; i < count; i += 32) {
        color = int3(palette[3*i], palette[3*i+1], palette[3*i+2]);
        gap = max(color-int3(low), int3(high)-color);
        far = uint(gap.x*gap.x+gap.y*gap.y+gap.z*gap.z);
        bound = min(bound, far);
    }
    if (simd_width == 32u) {
        bound = simd_min(bound);
    } else {
        bounds[lane] = bound;
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (lane == 0) {
            for (i = 1; i < 32; ++i) bounds[0] = min(bounds[0], bounds[i]);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        bound = bounds[0];
    }
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
        if (simd_width == 32u) {
            slot = kept+simd_prefix_exclusive_sum(keep);
            kept += simd_sum(keep);
        } else {
            prefix[lane] = keep;
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (lane == 0) {
                prefix[32] = 0;
                for (uint j = 0; j < 32; ++j) {
                    uint present = prefix[j];
                    prefix[j] = prefix[32];
                    prefix[32] += present;
                }
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            slot = kept+prefix[lane];
            kept += prefix[32];
            threadgroup_barrier(mem_flags::mem_threadgroup);
        }
        if (keep && slot < 64) candidates[slot] = uchar(i);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
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
