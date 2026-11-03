#include <metal_stdlib>
using namespace metal;
// The private uint32 workspace contains 16 member rows of 256 entries,
// 256 assignments, 16 counts, 16 minima, 16 maxima, then 32^3 seeds and
// 32^3 masks. Its 69,936 words are reserved by this policy alone.
static uint rbc_pack(constant uchar *p, uint i)
{
    return uint(p[3*i]) | (uint(p[3*i+1]) << 8)
        | (uint(p[3*i+2]) << 16) | (i << 24);
}
static int3 rbc_rgb(uint value)
{
    return int3(value & 255u, (value >> 8) & 255u,
        (value >> 16) & 255u);
}
static uint rbc_distance(int3 a, int3 b)
{
    int3 d;
    d = a - b;
    return uint(d.x*d.x + d.y*d.y + d.z*d.z);
}
kernel void rbc_assign(constant uchar *p [[buffer(0)]],
    constant uint& count [[buffer(1)]], device uint *table [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    uint groups, j, pivot, score, best, owner;
    int3 color;
    if (i >= count) return;
    groups = min(count, 16u);
    color = rbc_rgb(rbc_pack(p, i));
    best = 0xffffffffu;
    owner = 0;
    for (j = 0; j < groups; ++j) {
        pivot = j * count / groups;
        score = rbc_distance(color, rbc_rgb(rbc_pack(p, pivot)));
        if (score < best) { best = score; owner = j; }
    }
    table[4096u + i] = owner;
}
kernel void rbc_pack_clusters(constant uchar *p [[buffer(0)]],
    constant uint& count [[buffer(1)]], device uint *table [[buffer(3)]],
    uint cluster [[threadgroup_position_in_grid]],
    uint lane [[thread_index_in_threadgroup]])
{
    uint i, members, value;
    int3 low, high, color;
    if (lane != 0) return;
    low = int3(255); high = int3(0);
    members = 0; value = 0;
    for (i = 0; i < count; ++i) {
        if (table[4096u+i] != cluster) continue;
        value = rbc_pack(p, i);
        color = rbc_rgb(value);
        table[cluster*256u + members++] = value;
        low = min(low, color); high = max(high, color);
    }
    table[4352u+cluster] = members;
    table[4368u+cluster] = uint(low.x) | (uint(low.y)<<8)
        | (uint(low.z)<<16);
    table[4384u+cluster] = uint(high.x) | (uint(high.y)<<8)
        | (uint(high.z)<<16);
    // Four-wide reads repeat the last valid member at a short tail.
    for (i = members; i < ((members+3u)&~3u); ++i)
        table[cluster*256u+i] = value;
}

kernel void rbc_seed(constant uchar *palette [[buffer(0)]],
    constant uint& ncolors [[buffer(1)]], device uint *table [[buffer(3)]],
    uint cell [[thread_position_in_grid]])
{
    int3 q, c, d;
    uint best, minimum, distance, i, j, mask, limit;
    int3 lo, hi, low, high;
    if (cell >= 32768u) return;
    q = int3(2u*(cell>>10)+1u, 2u*((cell>>5)&31u)+1u,
        2u*(cell&31u)+1u)*255;
    // A seed need only be a valid palette color. Rational centers keep
    // construction exact without rounding a cell center to one byte.
    best = 0; minimum = 0xffffffffu;
    for (i = 0; i < ncolors; ++i) {
        c = rbc_rgb(rbc_pack(palette,i))*64;
        d = q-c;
        distance = uint(d.x*d.x+d.y*d.y+d.z*d.z);
        if (distance < minimum) { minimum = distance; best = i; }
    }
    table[4400u+cell] = rbc_pack(palette,best);
    lo = int3(cell>>10, (cell>>5)&31u, cell&31u)*8;
    hi = lo+7;
    c = rbc_rgb(rbc_pack(palette,best));
    d = max(abs(lo-c),abs(hi-c));
    limit = uint(d.x*d.x+d.y*d.y+d.z*d.z);
    mask = 0;
    // A whole cluster may be excluded only if its cell-to-box lower bound
    // exceeds the worst seed distance anywhere in this integer RGB cell.
    for (j = 0; j < min(ncolors,16u); ++j) {
        if (table[4352u+j] == 0) continue;
        low = rbc_rgb(table[4368u+j]);
        high = rbc_rgb(table[4384u+j]);
        d = max(max(low-hi,lo-high),int3(0));
        distance = uint(d.x*d.x+d.y*d.y+d.z*d.z);
        if (distance <= limit) mask |= 1u<<j;
    }
    table[37168u+cell] = mask;
}

static uint lookup_rgb8(uchar3 query, constant uchar *palette,
    uint ncolors, constant uchar *configuration,
    device const uchar *workspace)
{
    device const uint *table;
    uint best, j, members, at, d2, b2, cell, seed, mask;
    int3 q, d;
    uint4 packed, scores;
    int4 dr, dg, db;
    (void)palette; (void)ncolors; (void)configuration;
    table = (device const uint *)workspace;
    q = int3(query);
    cell = (uint(query.x>>3)<<10) | (uint(query.y>>3)<<5)
        | uint(query.z>>3);
    // Strict lower-bound pruning retains ties. Encoding the index in the
    // low byte of each score selects the first original palette entry.
    seed = table[4400u+cell];
    best = (rbc_distance(q,rbc_rgb(seed))<<8) | (seed>>24);
    mask = table[37168u+cell];
    while (mask != 0) {
        j = ctz(mask);
        mask &= mask-1u;
        members = table[4352u+j];
        if (members == 0) continue;
        b2 = best >> 8;
        // The cluster box gives an integer lower bound in RGB space.
        d = max(max(rbc_rgb(table[4368u+j])-q,
            q-rbc_rgb(table[4384u+j])), int3(0));
        d2 = uint(d.x*d.x + d.y*d.y + d.z*d.z);
        if (d2 > b2) continue;
        for (at = 0; at < members; at += 4) {
            packed = ((device const uint4 *)table)[j*64u + at/4u];
            dr = int4(packed & 255u) - q.x;
            dg = int4((packed >> 8)&255u) - q.y;
            db = int4((packed >> 16)&255u) - q.z;
            scores = (uint4(dr*dr + dg*dg + db*db)<<8) | (packed>>24);
            best = min(best, min(min(scores.x, scores.y),
                min(scores.z, scores.w)));
        }
    }
    return best & 255u;
}
