#include <metal_stdlib>
using namespace metal;
kernel void mahalanobis_build(constant uchar *palette [[buffer(0)]],
    constant uint& ncolors [[buffer(1)]], device uint *table [[buffer(3)]],
    uint at [[thread_position_in_grid]])
{
    uint i;
    if (at >= 256u) return;
    // Repeating the last valid color makes four-wide short tails safe.
    i = min(at, ncolors-1u);
    table[at] = uint(palette[3*i]) | (uint(palette[3*i+1])<<8)
        | (uint(palette[3*i+2])<<16) | (i<<24);
}
static uint lookup_rgb8(uchar3 query, constant uchar *palette,
    uint ncolors, constant uchar *configuration,
    device const uchar *workspace)
{
    device const uint4 *table;
    uint best, i;
    uint4 packed, scores;
    int4 dr, dg, db;
    (void)palette; (void)configuration;
    table = (device const uint4 *)workspace;
    best = 0xffffffffu;
    for (i = 0; i < (ncolors+3u)/4u; ++i) {
        packed = table[i];
        dr = int4(packed & 255u)-int(query.x);
        dg = int4((packed>>8)&255u)-int(query.y);
        db = int4((packed>>16)&255u)-int(query.z);
        // Distance dominates the key; equal distances use original index.
        scores = (uint4(dr*dr + dg*dg + db*db)<<8) | (packed>>24);
        best = min(best, min(min(scores.x, scores.y),
            min(scores.z, scores.w)));
    }
    return best&255u;
}

/* Palette packing is exact. Keep fractional coordinates in distance tests;
 * a packed integer score would discard the information diffusion needs. */
static uint mahalanobis_fractional(float3 query, uint count,
    device const uint *table)
{
    uint i, packed, best, index;
    float minimum, distance;
    float3 gap;

    best = 0;
    minimum = INFINITY;
    for (i = 0; i < count; ++i) {
        packed = table[i];
        index = packed >> 24;
        gap = query-float3(packed&255u, (packed>>8)&255u,
            (packed>>16)&255u);
        distance = dot(gap, gap);
        if (distance < minimum || (distance == minimum && index < best)) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}
static uint lookup_rgb8_fractional(float3 query, constant uchar *palette,
    uint count, constant uchar *configuration,
    device const uchar *workspace)
{
    device const uint *table;
    (void)palette; (void)configuration;
    table = (device const uint *)workspace;
    return mahalanobis_fractional(query,count,table);
}
