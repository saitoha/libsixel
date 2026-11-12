/* SPDX-License-Identifier: MIT */
/*
 * NONE lookup in the "vulkan.lookup-rgb8.v1" program ABI: the exhaustive
 * palette scan, first minimum (lowest index) on ties. It has no builder
 * and no workspace. The fractional entry follows
 * lookup-policy-none-fractional-gpu.metal: direct squared differences
 * avoid cancellation in |c|^2 - 2 q.c.
 */
uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint best, minimum, distance, i;
    ivec3 gap;

    best = 0u;
    minimum = 0xffffffffu;
    for (i = 0u; i < ncolors; ++i) {
        gap = ivec3(query) - ivec3(int(uint(lookup_palette[3u * i])),
            int(uint(lookup_palette[3u * i + 1u])),
            int(uint(lookup_palette[3u * i + 2u])));
        distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    return best;
}

uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uint best, i;
    float minimum, distance;
    vec3 gap;

    best = 0u;
    minimum = uintBitsToFloat(0x7f800000u);
    for (i = 0u; i < ncolors; ++i) {
        gap = query - vec3(float(uint(lookup_palette[3u * i])),
            float(uint(lookup_palette[3u * i + 1u])),
            float(uint(lookup_palette[3u * i + 2u])));
        distance = dot(gap, gap);
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    return best;
}
