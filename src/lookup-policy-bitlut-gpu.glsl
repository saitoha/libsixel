/* SPDX-License-Identifier: MIT */
/*
 * 5bit/6bit cell-center lookup in the "vulkan.lookup-rgb8.v1" program ABI,
 * a port of lookup-policy-bitlut-gpu.metal and its fractional form. The
 * builder fills one byte per cell with the palette entry nearest to that
 * cell's center, lowest index on ties. The table is immutable for a
 * palette, so it does not depend on the CPU first-query memo's traversal
 * history.
 *
 * Constants (words): 0 bits per channel. The workspace is the byte table
 * of 2^(3 * bits) cells.
 *
 * vulkan-builders: bitlut_build
 */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer BitlutWorkspace {
    uint8_t bitlut_table[];
};

#if defined(SIXEL_LOOKUP_BUILDER)
void bitlut_build()
{
    uint bits, cell, side, shift, half_step, best, distance, i, d;
    uvec3 address;
    ivec3 low, high, center2, delta;

    bits = lookup_constants[0];
    cell = gl_GlobalInvocationID.y * gl_NumWorkGroups.x *
        gl_WorkGroupSize.x + gl_GlobalInvocationID.x;
    if (cell >= 1u << (3u * bits)) {
        return;
    }
    side = 1u << bits;
    shift = 8u - bits;
    half_step = 1u << (shift - 1u);
    address = uvec3(cell >> (2u * bits), (cell >> bits) & (side - 1u),
        cell & (side - 1u));
    low = max(ivec3(address << shift) - int(half_step), ivec3(0));
    high = ivec3((address + 1u) << shift) - int(half_step) - 1;
    high = mix(high, ivec3(255), equal(address, uvec3(side - 1u)));
    /* Twice the center keeps half-byte endpoints exact, including the
     * clipped first cell and the saturated final cell. */
    center2 = low + high;
    best = 0u;
    distance = 0xffffffffu;
    for (i = 0u; i < lookup_count; ++i) {
        delta = center2 - 2 * ivec3(int(uint(lookup_palette[3u * i])),
            int(uint(lookup_palette[3u * i + 1u])),
            int(uint(lookup_palette[3u * i + 2u])));
        d = uint(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (d < distance) {
            distance = d;
            best = i;
        }
    }
    bitlut_table[cell] = uint8_t(best);
}
#endif

uint bitlut_entry(uvec3 address)
{
    uint bits = lookup_constants[0];

    return uint(bitlut_table[(address.x << (2u * bits)) |
        (address.y << bits) | address.z]);
}

/* Each channel rounds to its nearest cell. */
uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint bits, shift;

    bits = lookup_constants[0];
    shift = 8u - bits;
    return bitlut_entry(min((query + (1u << (shift - 1u))) >> shift,
        uvec3((1u << bits) - 1u)));
}

/* Bucket addressing keeps the query's sub-byte position up to the cell
 * boundary. A negative sum is clamped first because its uint conversion
 * is undefined; any such query lands in cell 0 either way. */
uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uint bits, shift;

    bits = lookup_constants[0];
    shift = 8u - bits;
    return bitlut_entry(min(uvec3(max(query + float(1u << (shift - 1u)),
        0.0) / float(1u << shift)), uvec3((1u << bits) - 1u)));
}
