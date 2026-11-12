/* SPDX-License-Identifier: MIT */
/*
 * Certified cell lookup in the "vulkan.lookup-rgb8.v1" program ABI,
 * a port of lookup-policy-certlut-gpu.metal. This policy owns both build
 * passes and queries; it neither includes nor calls another lookup.
 *
 * The private workspace contains 266,496 uint words: 256 packed palette
 * entries at [0, 256), 4096 rows of 64 packed candidates at [256, 262400),
 * and 4096 candidate counts at [262400, 266496). A packed entry contains
 * r | g << 8 | b << 16 | index << 24. A zero count requests a full palette
 * scan after candidate overflow. The one constant word selects fractional
 * cell bounds, including the interval above the last integer in a cell.
 *
 * vulkan-builders: certlut_pack certlut_build
 */
const uint certlut_cells = 4096u;
const uint certlut_palette_words = 256u;
const uint certlut_capacity = 64u;
const uint certlut_counts = certlut_palette_words
    + certlut_cells * certlut_capacity;

layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer CertlutWorkspace {
    uint certlut_table[];
};

ivec3 certlut_rgb(uint value)
{
    return ivec3(int(value & 255u), int((value >> 8) & 255u),
        int((value >> 16) & 255u));
}

uint certlut_distance2(ivec3 a, ivec3 b)
{
    ivec3 d;

    d = a - b;
    return uint(d.x * d.x + d.y * d.y + d.z * d.z);
}

/* Both the palette and candidate rows repeat their last valid entry at
 * short vector tails. Distance dominates the key; original index breaks
 * ties independently of the order in which candidates were retained. */
uint certlut_nearest(ivec3 query, uint count, uint row)
{
    uint best, i, base;
    uvec4 packed, scores;
    ivec4 dr, dg, db;

    best = 0xffffffffu;
    for (i = 0u; i < (count + 3u) / 4u; ++i) {
        base = row + 4u * i;
        packed = uvec4(certlut_table[base], certlut_table[base + 1u],
            certlut_table[base + 2u], certlut_table[base + 3u]);
        dr = ivec4(packed & 255u) - query.x;
        dg = ivec4((packed >> 8) & 255u) - query.y;
        db = ivec4((packed >> 16) & 255u) - query.z;
        scores = (uvec4(dr * dr + dg * dg + db * db) << 8) | (packed >> 24);
        best = min(best, min(min(scores.x, scores.y),
            min(scores.z, scores.w)));
    }
    return best & 255u;
}

#if defined(SIXEL_LOOKUP_BUILDER)
void certlut_pack()
{
    uint at, i;

    at = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    if (at >= certlut_palette_words) {
        return;
    }
    /* Repetition makes the final four-wide palette load safe. */
    i = min(at, lookup_count - 1u);
    certlut_table[at] = uint(lookup_palette[3u * i])
        | (uint(lookup_palette[3u * i + 1u]) << 8)
        | (uint(lookup_palette[3u * i + 2u]) << 16) | (i << 24);
}

/* Squared-distance differences are affine. Their exact box minimum is
 * attained at the corner chosen independently on each axis below. Retain
 * every color that can beat the center winner or win a first-index tie. */
void certlut_build()
{
    ivec3 low, high, a, b, corner;
    int margin;
    uint cell, best, i, kept, last, row, count;

    cell = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    if (cell >= certlut_cells) {
        return;
    }
    count = lookup_count;
    low = ivec3(int(cell >> 8), int((cell >> 4) & 15u),
        int(cell & 15u)) * 16;
    /* Closing fractional cells at the next integer boundary is
     * conservative; the final cell ends at 255 in both query modes. */
    high = min(low + 15 + int(lookup_constants[0]), ivec3(255));
    best = certlut_nearest(low + 8, count, 0u);
    a = certlut_rgb(certlut_table[best]);
    row = certlut_palette_words + cell * certlut_capacity;
    kept = 0u;
    last = certlut_table[best];
    for (i = 0u; i < count; ++i) {
        b = certlut_rgb(certlut_table[i]);
        corner = ivec3(a.x >= b.x ? low.x : high.x,
            a.y >= b.y ? low.y : high.y, a.z >= b.z ? low.z : high.z);
        /* Each squared distance fits in 18 bits, so the signed
         * difference remains exact in int32 throughout the byte domain. */
        margin = int(certlut_distance2(corner, b))
            - int(certlut_distance2(corner, a));
        if (i != best && (margin > 0 || (margin == 0 && i > best))) {
            continue;
        }
        if (kept < certlut_capacity) {
            certlut_table[row + kept] = certlut_table[i];
        }
        last = certlut_table[i];
        ++kept;
    }
    /* Overflow must never truncate the search to the stored candidates. */
    if (kept > certlut_capacity) {
        certlut_table[certlut_counts + cell] = 0u;
        return;
    }
    certlut_table[certlut_counts + cell] = kept;
    for (i = kept; i < ((kept + 3u) & ~3u); ++i) {
        certlut_table[row + i] = last;
    }
}
#endif

uint lookup_rgb8(uvec3 query, uint count)
{
    uint cell, kept, row;

    cell = ((query.x >> 4) << 8) | ((query.y >> 4) << 4) | (query.z >> 4);
    kept = certlut_table[certlut_counts + cell];
    if (kept == 0u) {
        return certlut_nearest(ivec3(query), count, 0u);
    }
    row = certlut_palette_words + cell * certlut_capacity;
    if (kept == 1u) {
        return certlut_table[row] >> 24;
    }
    return certlut_nearest(ivec3(query), kept, row);
}

/* Keep the unrounded query in squared differences. An integer packed
 * score would discard the fractional information diffusion needs. */
uint certlut_fractional(vec3 query, uint count, uint row)
{
    uint i, packed, best, index;
    float minimum, distance;
    vec3 gap;

    best = 0u;
    minimum = uintBitsToFloat(0x7f800000u);
    for (i = 0u; i < count; ++i) {
        packed = certlut_table[row + i];
        index = packed >> 24;
        gap = query - vec3(certlut_rgb(packed));
        distance = dot(gap, gap);
        if (distance < minimum || (distance == minimum && index < best)) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}

uint lookup_rgb8_fractional(vec3 query, uint count)
{
    uvec3 code;
    uint cell, kept, row;

    /* Clamp before unsigned conversion, which is undefined for negative
     * floats. Only cell addressing is quantized; distance is not. */
    code = uvec3(clamp(query, 0.0, 255.0)) >> 4;
    cell = (code.x << 8) | (code.y << 4) | code.z;
    kept = certlut_table[certlut_counts + cell];
    if (kept == 0u) {
        return certlut_fractional(query, count, 0u);
    }
    row = certlut_palette_words + cell * certlut_capacity;
    if (kept == 1u) {
        return certlut_table[row] >> 24;
    }
    return certlut_fractional(query, kept, row);
}
