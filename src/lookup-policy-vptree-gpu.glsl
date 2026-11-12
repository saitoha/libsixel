/* SPDX-License-Identifier: MIT */
/*
 * Vantage-point candidate lookup in the "vulkan.lookup-rgb8.v1" program
 * ABI, a port of lookup-policy-vptree-gpu.metal. This implementation owns
 * its builders, table layout and queries together; it does not include or
 * call another lookup implementation.
 *
 * The workspace is 163840 uint words, 640 KiB. Words [0, 65536) hold one
 * distance-sorted row of stride 256 per palette entry, each key packing
 * squared distance << 8 | index, including short palettes and padded
 * tails. Words [65536, 131072) hold the same rows as packed RGB words
 * r | g << 8 | b << 16 | index << 24. Words [131072, 163840) hold the
 * nearest entry to the center of each 8^3 byte cell, the 32^3 seeds.
 * Byte queries use integer arithmetic without geometry tolerances. There
 * are no constants.
 *
 * vulkan-builders: vptree_build_rows vptree_build_seeds
 */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer VptreeWorkspace {
    uint vptree_table[];
};

uint vptree_distance(uvec3 query, uint i)
{
    ivec3 gap;

    gap = ivec3(query) - ivec3(int(uint(lookup_palette[3u * i])),
        int(uint(lookup_palette[3u * i + 1u])),
        int(uint(lookup_palette[3u * i + 2u])));
    return uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
}

#if defined(SIXEL_LOOKUP_BUILDER)
shared uint vptree_keys[256];
shared uint vptree_own_at;

/* One workgroup sorts one palette row with a bitonic network. The pass
 * width is the smallest power of two, at least 32, that holds the row;
 * the padded lanes sort last. */
void vptree_build_rows()
{
    uvec3 center;
    uint row, lane, width, step, other, a, b, at, key, index, rgb, ncolors;
    bool ascending;

    row = gl_WorkGroupID.x;
    lane = gl_LocalInvocationID.x;
    ncolors = lookup_count;
    center = uvec3(uint(lookup_palette[row * 3u]),
        uint(lookup_palette[row * 3u + 1u]),
        uint(lookup_palette[row * 3u + 2u]));
    vptree_keys[lane] = lane < ncolors
        ? (vptree_distance(center, lane) << 8) | lane
        : 0xffffffffu;
    memoryBarrierShared();
    barrier();
    for (width = 2u; width < 2u * ncolors; width <<= 1) {
        for (step = width >> 1; step > 0u; step >>= 1) {
            other = lane ^ step;
            if (other > lane) {
                a = vptree_keys[lane];
                b = vptree_keys[other];
                ascending = (lane & width) == 0u;
                vptree_keys[lane] = ascending ? min(a, b) : max(a, b);
                vptree_keys[other] = ascending ? max(a, b) : min(a, b);
            }
            memoryBarrierShared();
            barrier();
        }
    }
    /* Original indexes distinguish the center even among duplicate colors.
     * Locate and omit it in parallel, without a serial compaction pass. */
    if ((vptree_keys[lane] & 255u) == row) {
        vptree_own_at = lane;
    }
    memoryBarrierShared();
    barrier();
    at = min(lane, ncolors > 1u ? ncolors - 2u : 0u);
    if (at >= vptree_own_at && ncolors > 1u) {
        ++at;
    }
    key = vptree_keys[at];
    index = key & 255u;
    rgb = uint(lookup_palette[index * 3u])
        | (uint(lookup_palette[index * 3u + 1u]) << 8)
        | (uint(lookup_palette[index * 3u + 2u]) << 16) | (index << 24);
    vptree_table[row * 256u + lane] = key;
    vptree_table[65536u + row * 256u + lane] = rgb;
}

void vptree_build_seeds()
{
    ivec3 q, color, gap;
    uint cell, best, minimum, distance, i;

    cell = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    if (cell >= 32768u) {
        return;
    }
    /* Exact rational centers scaled by 64. The largest squared distance
     * is below 2^30, so even signed intermediate arithmetic is exact. */
    q = ivec3(int(2u * (cell >> 10) + 1u),
        int(2u * ((cell >> 5) & 31u) + 1u),
        int(2u * (cell & 31u) + 1u)) * 255;
    best = 0u;
    minimum = 0xffffffffu;
    for (i = 0u; i < lookup_count; ++i) {
        color = ivec3(int(uint(lookup_palette[i * 3u])),
            int(uint(lookup_palette[i * 3u + 1u])),
            int(uint(lookup_palette[i * 3u + 2u]))) * 64;
        gap = q - color;
        distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    vptree_table[131072u + cell] = best;
}
#endif

/* The seed's own key starts the search. Every color no worse than it lies
 * within twice |q-c|, so the complete distance-sorted row of the seed is
 * scanned up to four times its squared distance. Equality must survive
 * for ties; the smallest packed key keeps the lowest index. */
uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint center, best, limit, at, key, cell, base;
    uvec4 packed, scores;
    ivec4 dr, dg, db;

    cell = ((query.x >> 3) << 10) | ((query.y >> 3) << 5) | (query.z >> 3);
    center = vptree_table[131072u + cell];
    best = (vptree_distance(query, center) << 8) | center;
    limit = 4u * (best >> 8);
    for (at = 0u; at + 1u < ncolors; at += 4u) {
        if ((vptree_table[center * 256u + at] >> 8) > limit) {
            break;
        }
        base = 65536u + center * 256u + at;
        packed = uvec4(vptree_table[base], vptree_table[base + 1u],
            vptree_table[base + 2u], vptree_table[base + 3u]);
        dr = ivec4(packed & 255u) - int(query.x);
        dg = ivec4((packed >> 8) & 255u) - int(query.y);
        db = ivec4((packed >> 16) & 255u) - int(query.z);
        scores = (uvec4(dr * dr + dg * dg + db * db) << 8) | (packed >> 24);
        key = min(min(scores.x, scores.y), min(scores.z, scores.w));
        best = min(best, key);
    }
    return best & 255u;
}

/* Fractional code-value queries retain the complete distance-sorted rows.
 * Seeds only select a starting point; they do not certify a byte cell.
 * A conservative unit of slack protects the triangle bound from float
 * rounding. It can add candidates, never remove a nearest-color tie. The
 * query is clamped before the uint conversion, which is undefined for a
 * negative float. */
uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uint center, best, at, cell, packed, index;
    uvec3 code;
    vec3 gap;
    float minimum, distance, limit;

    code = uvec3(clamp(query, 0.0, 255.0)) >> 3;
    cell = (code.x << 10) | (code.y << 5) | code.z;
    center = vptree_table[131072u + cell];
    gap = query - vec3(float(uint(lookup_palette[3u * center])),
        float(uint(lookup_palette[3u * center + 1u])),
        float(uint(lookup_palette[3u * center + 2u])));
    minimum = dot(gap, gap);
    limit = 4.0 * minimum + 1.0;
    best = center;
    for (at = 0u; at + 1u < ncolors; ++at) {
        if (float(vptree_table[center * 256u + at] >> 8) > limit) {
            break;
        }
        packed = vptree_table[65536u + center * 256u + at];
        index = packed >> 24;
        gap = query - vec3(float(packed & 255u), float((packed >> 8) & 255u),
            float((packed >> 16) & 255u));
        distance = dot(gap, gap);
        if (distance < minimum || (distance == minimum && index < best)) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}
