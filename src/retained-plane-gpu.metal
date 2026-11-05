/* SPDX-License-Identifier: MIT */
#include <metal_stdlib>
using namespace metal;

struct RetainedParams {
    uint width, height, x, y, stride, count, record_paint;
    uchar palette[768];
};

/* Every source lane writes its candidate at the canvas origin. The backend
 * reconciles earlier dirty ranges before dispatch, so untouched canvas bytes
 * remain committed history. Keeps copy exact bytes and validity. */
kernel void retained_expand(
    device const uchar *rgb [[buffer(0)]],
    device const uchar *valid [[buffer(1)]],
    device uchar *next_rgb [[buffer(2)]],
    device uchar *next_valid [[buffer(3)]],
    device const uchar *indexes [[buffer(4)]],
    device const uchar *skip [[buffer(5)]],
    device uchar *paint [[buffer(6)]],
    device uint2 *stats [[buffer(7)]],
    constant RetainedParams &p [[buffer(8)]],
    uint gid [[thread_position_in_grid]],
    uint lane [[thread_index_in_simdgroup]],
    uint simd [[simdgroup_index_in_threadgroup]],
    uint simds [[simdgroups_per_threadgroup]],
    uint group [[threadgroup_position_in_grid]])
{
    threadgroup uint2 reductions[32];
    uint2 result = uint2(0);
    if (gid < p.count) {
        uint target = (p.y + gid / p.width) * p.stride + p.x + gid % p.width;
        uint state = skip[gid];
        uint color = state == 0 ? indexes[gid] : 0;
        for (uint c = 0; c < 3; ++c)
            next_rgb[target * 3 + c] = state == 0
                ? p.palette[color * 3 + c] : rgb[target * 3 + c];
        next_valid[target] = state == 0 ? 1 : valid[target];
        result = uint2(state == 0, state == 3);
        if (p.record_paint != 0) paint[gid] = state == 0;
    }
    result = uint2(simd_sum(result.x), simd_or(result.y));
    if (lane == 0) reductions[simd] = result;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (simd == 0) {
        result = lane < simds ? reductions[lane] : uint2(0);
        result = uint2(simd_sum(result.x), simd_or(result.y));
        if (lane == 0) stats[group] = result;
    }
}
