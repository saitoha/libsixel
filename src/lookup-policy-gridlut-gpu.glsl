/* SPDX-License-Identifier: MIT */
/*
 * Grid candidate lookup in the "vulkan.lookup-rgb8.v1" program ABI.
 * Byte construction and lookup port lookup-policy-gridlut-gpu.metal;
 * fractional construction and lookup port its fractional Metal source.
 * This policy owns its builder, table and both queries independently.
 *
 * The workspace preserves the Metal layout: 4096 cells of 65 bytes, each
 * with one count followed by 64 palette indexes. A zero count requests a
 * full palette scan after candidate overflow. The single constant word
 * is nonzero for fractional queries, whose cells include the interval
 * above the last integer. Byte plans retain the legacy integer bounds.
 *
 * vulkan-builders: gridlut_build
 */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer GridlutWorkspace {
    uint8_t gridlut_table[];
};

ivec3 gridlut_color(uint index)
{
    return ivec3(int(uint(lookup_palette[3u * index])),
        int(uint(lookup_palette[3u * index + 1u])),
        int(uint(lookup_palette[3u * index + 2u])));
}

#if defined(SIXEL_LOOKUP_BUILDER)
/* One invocation constructs one cell. The smallest farthest-color
 * distance bounds the nearest color at every point in the box; a color
 * is excluded only when its nearest-box distance exceeds that bound. */
void gridlut_build()
{
    ivec3 low, high, color, gap;
    uint cell, row, bound, far_distance, near_distance, count, i;

    cell = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    if (cell >= 4096u) {
        return;
    }
    row = cell * 65u;
    low = ivec3(int(cell >> 8), int((cell >> 4) & 15u),
        int(cell & 15u)) * 16;
    /* Fractional plans close the upper face conservatively. The last
     * cell stops at 255 for both query kinds. */
    high = min(low + 15 + int(lookup_constants[0]), ivec3(255));
    bound = 0xffffffffu;
    for (i = 0u; i < lookup_count; ++i) {
        color = gridlut_color(i);
        gap = max(color - low, high - color);
        far_distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        bound = min(bound, far_distance);
    }
    count = 0u;
    for (i = 0u; i < lookup_count; ++i) {
        color = gridlut_color(i);
        gap = max(max(low - color, color - high), ivec3(0));
        near_distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        if (near_distance <= bound) {
            if (count < 64u) {
                gridlut_table[row + 1u + count] = uint8_t(i);
            }
            ++count;
        }
    }
    /* Do not truncate overflow to the first 64 colors. The original
     * palette remains the source of the exhaustive query in that case. */
    gridlut_table[row] = uint8_t(count <= 64u ? count : 0u);
}
#endif

/* Candidates are stored in palette order, so a strict improvement keeps
 * the lowest original index on ties, as in the legacy Metal byte query. */
uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint cell, row, count, best, minimum, index, distance, i;
    ivec3 gap;

    cell = ((query.x >> 4) << 8) | ((query.y >> 4) << 4) | (query.z >> 4);
    row = cell * 65u;
    count = uint(gridlut_table[row]);
    if (count == 1u) {
        return uint(gridlut_table[row + 1u]);
    }
    best = 0u;
    minimum = 0xffffffffu;
    for (i = 0u; i < (count != 0u ? count : ncolors); ++i) {
        index = count != 0u ? uint(gridlut_table[row + 1u + i]) : i;
        gap = ivec3(query) - gridlut_color(index);
        distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        if (distance < minimum) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}

/* Only the cell address is quantized. Distances preserve the fractional
 * query, including the strip covered by the expanded builder bounds. */
uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uvec3 address;
    uint cell, row, count, best, index, i;
    float minimum, distance;
    vec3 gap;

    /* Negative float-to-uint conversion is undefined; clamp first. */
    address = uvec3(clamp(query, 0.0, 255.0)) >> 4;
    cell = (address.x << 8) | (address.y << 4) | address.z;
    row = cell * 65u;
    count = uint(gridlut_table[row]);
    if (count == 1u) {
        return uint(gridlut_table[row + 1u]);
    }
    minimum = uintBitsToFloat(0x7f800000u);
    best = 0u;
    for (i = 0u; i < (count != 0u ? count : ncolors); ++i) {
        index = count != 0u ? uint(gridlut_table[row + 1u + i]) : i;
        gap = query - vec3(gridlut_color(index));
        distance = dot(gap, gap);
        if (distance < minimum) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}
