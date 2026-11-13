/* One workgroup owns one 8x8 block. Bounds and overflow rules match the
 * matrix consumer; only the final candidate distances use scalar math. */
kernel void lookup_blockcull_scalar(
    device uchar *result [[buffer(0)]],
    device const uchar *pixels [[buffer(1)]],
    constant uchar *palette [[buffer(2)]],
    device const uchar *transparent_mask [[buffer(3)]],
    device const uchar *blue_noise [[buffer(4)]],
    device const uchar *gradient [[buffer(5)]],
    constant Params& params [[buffer(6)]],
    device const uchar *accumulation [[buffer(7)]],
    device const uchar *accumulation_valid_mask [[buffer(8)]],
    device uchar *accumulation_result_mask [[buffer(9)]],
    uint tile [[threadgroup_position_in_grid]],
    uint lane [[thread_index_in_threadgroup]],
    uint simd_width [[threads_per_simdgroup]])
{
    threadgroup uchar candidates[64];
    threadgroup uint3 lows[32], highs[32];
    threadgroup uint bounds[32], prefix[33];
    uchar3 query[2], source[2];
    bool done[2];
    uint at[2];
    bool inside[2];
    uint2 xy;
    uint3 low, high;
    int3 color, gap;
    uint h, i, base, slot, keep, kept, bound, near, far, index, best;
    uint minimum, distance;
    int3 difference;

    low = uint3(255);
    high = uint3(0);
    for (h = 0; h < 2; ++h) {
        i = lane+h*32;
        xy = uint2(tile % ((params.width+7)/8),
            tile / ((params.width+7)/8))*8+uint2(i%8, i/8);
        inside[h] = all(xy < uint2(params.width, params.height));
        at[h] = inside[h] ? xy.y*params.width+xy.x : params.pixel_count;
        done[h] = prepare_query(result, pixels, transparent_mask,
            blue_noise, gradient, params, accumulation,
            accumulation_valid_mask, accumulation_result_mask, at[h],
            query[h], source[h]);
        if (!done[h]) {
            low = min(low, uint3(query[h]));
            high = max(high, uint3(query[h]));
        }
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
    for (i = lane; i < params.ncolors; i += 32) {
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
    for (base = 0; base < params.ncolors && kept <= 64; base += 32) {
        i = base+lane;
        keep = 0;
        if (i < params.ncolors) {
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
        if (done[h]) continue;
        best = 0;
        minimum = UINT_MAX;
        /* Overflow scans the whole palette, never a truncated list. */
        for (i = 0; i < (kept > 64 ? params.ncolors : kept); ++i) {
            index = kept > 64 ? i : uint(candidates[i]);
            difference = int3(query[h]) - int3(palette[3*index],
                palette[3*index+1], palette[3*index+2]);
            distance = uint(difference.x*difference.x +
                difference.y*difference.y + difference.z*difference.z);
            if (distance < minimum) { minimum = distance; best = index; }
        }
        finish_pixel(result, params, accumulation, accumulation_valid_mask,
            accumulation_result_mask, at[h], source[h], best);
    }
}
