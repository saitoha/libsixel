// SPDX-License-Identifier: MIT
// This vptree implementation owns its builder, layout and query together.
// It does not include or call another lookup implementation.
#include <metal_stdlib>
using namespace metal;

static uint vptree_distance(constant uchar *palette, uchar3 query, uint i)
{
    int3 gap;

    gap = int3(query) - int3(palette[3u * i], palette[3u * i + 1u],
        palette[3u * i + 2u]);
    return uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
}
/* Distance rows, packed RGB rows and 32^3 seeds share a 640 KiB table.
 * Queries use integer arithmetic without geometry tolerances.
 * Rows have stride 256, including short palettes and padded tails. */
kernel void vptree_build_rows(
    constant uchar *palette [[buffer(0)]],
    constant uint& ncolors [[buffer(1)]],
    device uint *table [[buffer(3)]],
    uint row [[threadgroup_position_in_grid]],
    uint lane [[thread_index_in_threadgroup]])
{
    threadgroup uint keys[256];
    threadgroup uint own_at;
    uchar3 center;
    uint width, step, other, a, b, at, key, index, rgb;
    bool ascending;

    center = uchar3(palette[row * 3u], palette[row * 3u + 1u],
                    palette[row * 3u + 2u]);
    keys[lane] = lane < ncolors
        ? (vptree_distance(palette, center, lane) << 8) | lane
        : 0xffffffffu;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (width = 2u; width < 2u * ncolors; width <<= 1) {
        for (step = width >> 1; step > 0u; step >>= 1) {
            other = lane ^ step;
            if (other > lane) {
                a = keys[lane];
                b = keys[other];
                ascending = (lane & width) == 0u;
                keys[lane] = ascending ? min(a, b) : max(a, b);
                keys[other] = ascending ? max(a, b) : min(a, b);
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
        }
    }
    /* Original indexes distinguish the center even among duplicate colors.
     * Locate and omit it in parallel, without a serial compaction pass. */
    if ((keys[lane] & 255u) == row) {
        own_at = lane;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    at = min(lane, ncolors > 1u ? ncolors - 2u : 0u);
    at += at >= own_at && ncolors > 1u;
    key = keys[at];
    index = key & 255u;
    rgb = uint(palette[index * 3u])
        | (uint(palette[index * 3u + 1u]) << 8)
        | (uint(palette[index * 3u + 2u]) << 16) | (index << 24);
    table[row * 256u + lane] = key;
    table[65536u + row * 256u + lane] = rgb;
}
kernel void vptree_build_seeds(
    constant uchar *palette [[buffer(0)]],
    constant uint& ncolors [[buffer(1)]],
    device uint *table [[buffer(3)]],
    uint cell [[thread_position_in_grid]])
{
    int3 q, color, gap;
    uint best, minimum, distance, i;

    if (cell >= 32768u) {
        return;
    }
    /* Exact rational centers scaled by 64. The largest squared distance
     * is below 2^30, so even signed intermediate arithmetic is exact. */
    q = int3(2u * (cell >> 10) + 1u,
             2u * ((cell >> 5) & 31u) + 1u,
             2u * (cell & 31u) + 1u) * 255;
    best = 0u;
    minimum = 0xffffffffu;
    for (i = 0u; i < ncolors; ++i) {
        color = int3(palette[i * 3u], palette[i * 3u + 1u],
                     palette[i * 3u + 2u]) * 64;
        gap = q - color;
        distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    table[131072u + cell] = best;
}
static uint lookup_rgb8(uchar3 query, constant uchar *palette,
    uint ncolors, constant uchar *configuration,
    device const uchar *workspace)
{
    device const uint *table;
    uint center, best, limit, at, key, cell;
    uint4 packed, scores;
    int4 dr, dg, db;

    (void)configuration;
    table = (device const uint *)workspace;
    cell = (uint(query.x >> 3) << 10) | (uint(query.y >> 3) << 5)
        | uint(query.z >> 3);
    center = table[131072u + cell];
    best = (vptree_distance(palette, query, center) << 8) | center;
    /* Every color no worse than c lies within twice |q-c|. Equality must
     * survive for ties. One scan suffices because the row is complete. */
    limit = 4u * (best >> 8);
    for (at = 0u; at + 1u < ncolors; at += 4u) {
        if ((table[center * 256u + at] >> 8) > limit) {
            break;
        }
        packed = ((device const uint4 *)table)[
            16384u + center * 64u + at / 4u];
        dr = int4(packed & 255u) - int(query.x);
        dg = int4((packed >> 8) & 255u) - int(query.y);
        db = int4((packed >> 16) & 255u) - int(query.z);
        scores = (uint4(dr * dr + dg * dg + db * db) << 8)
            | (packed >> 24);
        key = min(min(scores.x, scores.y), min(scores.z, scores.w));
        best = min(best, key);
    }
    return best & 255u;
}
