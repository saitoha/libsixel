/* SPDX-License-Identifier: MIT */
/*
 * Mahalanobis RGB8 lookup in the "vulkan.lookup-rgb8.v1" program ABI,
 * a port of lookup-policy-mahalanobis-gpu.metal. This policy owns its
 * builder and both queries; the byte metric is exhaustive unweighted RGB,
 * without covariance arithmetic or another policy's search.
 *
 * The private workspace contains 256 uint words, each packing
 * r | g << 8 | b << 16 | index << 24. Unused entries repeat the last valid
 * color so four-wide byte queries can read a short tail safely.
 *
 * vulkan-builders: mahalanobis_build
 */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer MahalanobisWorkspace {
    uint mahalanobis_table[];
};

#if defined(SIXEL_LOOKUP_BUILDER)
void mahalanobis_build()
{
    uint at, i;

    at = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    if (at >= 256u) {
        return;
    }
    i = min(at, lookup_count - 1u);
    mahalanobis_table[at] = uint(lookup_palette[3u * i])
        | (uint(lookup_palette[3u * i + 1u]) << 8)
        | (uint(lookup_palette[3u * i + 2u]) << 16) | (i << 24);
}
#endif

/* Distance dominates the packed key; equal distances select the first
 * original palette index, including repeated entries at a short tail. */
uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint best, i, base;
    uvec4 packed, scores;
    ivec4 dr, dg, db;

    best = 0xffffffffu;
    for (i = 0u; i < (ncolors + 3u) / 4u; ++i) {
        base = 4u * i;
        packed = uvec4(mahalanobis_table[base],
            mahalanobis_table[base + 1u], mahalanobis_table[base + 2u],
            mahalanobis_table[base + 3u]);
        dr = ivec4(packed & 255u) - int(query.x);
        dg = ivec4((packed >> 8) & 255u) - int(query.y);
        db = ivec4((packed >> 16) & 255u) - int(query.z);
        scores = (uvec4(dr * dr + dg * dg + db * db) << 8) | (packed >> 24);
        best = min(best, min(min(scores.x, scores.y),
            min(scores.z, scores.w)));
    }
    return best & 255u;
}

/* Preserve the unrounded fractional query in squared differences.
 * Packing its score into an integer would lose diffusion information. */
uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uint i, packed, best, index;
    float minimum, distance;
    vec3 gap;

    best = 0u;
    minimum = uintBitsToFloat(0x7f800000u);
    for (i = 0u; i < ncolors; ++i) {
        packed = mahalanobis_table[i];
        index = packed >> 24;
        gap = query - vec3(packed & 255u, (packed >> 8) & 255u,
            (packed >> 16) & 255u);
        distance = dot(gap, gap);
        if (distance < minimum || (distance == minimum && index < best)) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}
