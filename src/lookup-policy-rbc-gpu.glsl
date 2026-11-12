/* SPDX-License-Identifier: MIT */
/*
 * Random Ball Cover candidate lookup in the "vulkan.lookup-rgb8.v1"
 * program ABI, a port of lookup-policy-rbc-gpu.metal. This implementation
 * owns its builders, table layout and queries together; it does not
 * include or call another lookup implementation.
 *
 * The private workspace is 69,936 uint words: 16 member rows of 256 packed
 * entries r | g << 8 | b << 16 | index << 24 at [0, 4096), 256 cluster
 * assignments at [4096, 4352), 16 member counts at [4352, 4368), 16 box
 * minima at [4368, 4384), 16 box maxima at [4384, 4400), then 32^3 packed
 * seeds at [4400, 37168) and 32^3 cluster masks at [37168, 69936). The one
 * constant word is nonzero when the plan serves fractional queries, whose
 * masks cover the continuous cell.
 *
 * vulkan-builders: rbc_assign rbc_pack_clusters rbc_seed
 */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer RbcWorkspace {
    uint rbc_table[];
};

uint rbc_pack(uint i)
{
    return uint(lookup_palette[3u * i])
        | (uint(lookup_palette[3u * i + 1u]) << 8)
        | (uint(lookup_palette[3u * i + 2u]) << 16) | (i << 24);
}

ivec3 rbc_rgb(uint value)
{
    return ivec3(int(value & 255u), int((value >> 8) & 255u),
        int((value >> 16) & 255u));
}

uint rbc_distance(ivec3 a, ivec3 b)
{
    ivec3 d;

    d = a - b;
    return uint(d.x * d.x + d.y * d.y + d.z * d.z);
}

#if defined(SIXEL_LOOKUP_BUILDER)
/* One invocation assigns one palette entry to its nearest of at most 16
 * evenly spaced pivots; the first pivot keeps a tie. */
void rbc_assign()
{
    uint i, count, groups, j, pivot, score, best, owner;
    ivec3 color;

    i = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    count = lookup_count;
    if (i >= count) {
        return;
    }
    groups = min(count, 16u);
    color = rbc_rgb(rbc_pack(i));
    best = 0xffffffffu;
    owner = 0u;
    for (j = 0u; j < groups; ++j) {
        pivot = j * count / groups;
        score = rbc_distance(color, rbc_rgb(rbc_pack(pivot)));
        if (score < best) {
            best = score;
            owner = j;
        }
    }
    rbc_table[4096u + i] = owner;
}

/* One workgroup per cluster; its first lane packs the members in palette
 * order and records their bounding box. */
void rbc_pack_clusters()
{
    uint cluster, i, members, value, count;
    ivec3 low, high, color;

    cluster = gl_WorkGroupID.x;
    if (gl_LocalInvocationID.x != 0u) {
        return;
    }
    count = lookup_count;
    low = ivec3(255);
    high = ivec3(0);
    members = 0u;
    value = 0u;
    for (i = 0u; i < count; ++i) {
        if (rbc_table[4096u + i] != cluster) {
            continue;
        }
        value = rbc_pack(i);
        color = rbc_rgb(value);
        rbc_table[cluster * 256u + members] = value;
        ++members;
        low = min(low, color);
        high = max(high, color);
    }
    rbc_table[4352u + cluster] = members;
    rbc_table[4368u + cluster] = uint(low.x) | (uint(low.y) << 8)
        | (uint(low.z) << 16);
    rbc_table[4384u + cluster] = uint(high.x) | (uint(high.y) << 8)
        | (uint(high.z) << 16);
    /* Four-wide reads repeat the last valid member at a short tail. */
    for (i = members; i < ((members + 3u) & ~3u); ++i) {
        rbc_table[cluster * 256u + i] = value;
    }
}

void rbc_seed()
{
    ivec3 q, c, d, lo, hi, low, high;
    uint cell, best, minimum, distance, i, j, mask, limit, ncolors;

    cell = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    if (cell >= 32768u) {
        return;
    }
    ncolors = lookup_count;
    q = ivec3(int(2u * (cell >> 10) + 1u), int(2u * ((cell >> 5) & 31u) + 1u),
        int(2u * (cell & 31u) + 1u)) * 255;
    /* A seed need only be a valid palette color. Rational centers keep
     * construction exact without rounding a cell center to one byte. */
    best = 0u;
    minimum = 0xffffffffu;
    for (i = 0u; i < ncolors; ++i) {
        c = rbc_rgb(rbc_pack(i)) * 64;
        d = q - c;
        distance = uint(d.x * d.x + d.y * d.y + d.z * d.z);
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    rbc_table[4400u + cell] = rbc_pack(best);
    lo = ivec3(int(cell >> 10), int((cell >> 5) & 31u), int(cell & 31u)) * 8;
    hi = min(lo + 7 + int(lookup_constants[0]), ivec3(255));
    c = rbc_rgb(rbc_pack(best));
    d = max(abs(lo - c), abs(hi - c));
    limit = uint(d.x * d.x + d.y * d.y + d.z * d.z);
    mask = 0u;
    /* A whole cluster may be excluded only if its cell-to-box lower bound
     * exceeds the worst seed distance anywhere in the selected integer or
     * continuous RGB cell. */
    for (j = 0u; j < min(ncolors, 16u); ++j) {
        if (rbc_table[4352u + j] == 0u) {
            continue;
        }
        low = rbc_rgb(rbc_table[4368u + j]);
        high = rbc_rgb(rbc_table[4384u + j]);
        d = max(max(low - hi, lo - high), ivec3(0));
        distance = uint(d.x * d.x + d.y * d.y + d.z * d.z);
        if (distance <= limit) {
            mask |= 1u << j;
        }
    }
    rbc_table[37168u + cell] = mask;
}
#endif

/* Strict lower-bound pruning retains ties. Encoding the index in the low
 * byte of each score selects the first original palette entry. */
uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint best, j, members, at, d2, b2, cell, seed, mask, base;
    ivec3 q, d;
    uvec4 packed, scores;
    ivec4 dr, dg, db;

    q = ivec3(query);
    cell = ((query.x >> 3) << 10) | ((query.y >> 3) << 5) | (query.z >> 3);
    seed = rbc_table[4400u + cell];
    best = (rbc_distance(q, rbc_rgb(seed)) << 8) | (seed >> 24);
    mask = rbc_table[37168u + cell];
    while (mask != 0u) {
        j = uint(findLSB(mask));
        mask &= mask - 1u;
        members = rbc_table[4352u + j];
        if (members == 0u) {
            continue;
        }
        b2 = best >> 8;
        /* The cluster box gives an integer lower bound in RGB space. */
        d = max(max(rbc_rgb(rbc_table[4368u + j]) - q,
            q - rbc_rgb(rbc_table[4384u + j])), ivec3(0));
        d2 = uint(d.x * d.x + d.y * d.y + d.z * d.z);
        if (d2 > b2) {
            continue;
        }
        for (at = 0u; at < members; at += 4u) {
            base = j * 256u + at;
            packed = uvec4(rbc_table[base], rbc_table[base + 1u],
                rbc_table[base + 2u], rbc_table[base + 3u]);
            dr = ivec4(packed & 255u) - q.x;
            dg = ivec4((packed >> 8) & 255u) - q.y;
            db = ivec4((packed >> 16) & 255u) - q.z;
            scores = (uvec4(dr * dr + dg * dg + db * db) << 8)
                | (packed >> 24);
            best = min(best, min(min(scores.x, scores.y),
                min(scores.z, scores.w)));
        }
    }
    return best & 255u;
}

/* The mask builder covers the continuous cell for fractional plans.
 * Runtime box bounds and member distances retain the unrounded query. The
 * query is clamped before the uint conversion, which is undefined for a
 * negative float. */
uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uvec3 code;
    uint cell, seed, best, mask, j, at, packed, index;
    float minimum, distance;
    vec3 gap;

    code = uvec3(clamp(query, 0.0, 255.0)) >> 3;
    cell = (code.x << 10) | (code.y << 5) | code.z;
    seed = rbc_table[4400u + cell];
    best = seed >> 24;
    gap = query - vec3(rbc_rgb(seed));
    minimum = dot(gap, gap);
    mask = rbc_table[37168u + cell];
    while (mask != 0u) {
        j = uint(findLSB(mask));
        mask &= mask - 1u;
        gap = max(max(vec3(rbc_rgb(rbc_table[4368u + j])) - query,
            query - vec3(rbc_rgb(rbc_table[4384u + j]))), vec3(0.0));
        /* A conservative slack protects pruning near float ties only. */
        if (dot(gap, gap) > minimum + 1.0) {
            continue;
        }
        for (at = 0u; at < rbc_table[4352u + j]; ++at) {
            packed = rbc_table[j * 256u + at];
            index = packed >> 24;
            gap = query - vec3(rbc_rgb(packed));
            distance = dot(gap, gap);
            if (distance < minimum ||
                    (distance == minimum && index < best)) {
                minimum = distance;
                best = index;
            }
        }
    }
    return best;
}
