/* SPDX-License-Identifier: MIT */
#include <metal_stdlib>
using namespace metal;
kernel void lookup_5bit_fractional_build(
    constant uchar *palette [[buffer(0)]],
    constant uint &count [[buffer(1)]],
    device uchar *table [[buffer(3)]],
    uint cell [[thread_position_in_grid]])
{
    uint bits, side, shift, half_step, best, distance, i, d;
    uint3 address;
    int3 low, high, center2, delta;
    /* This builder owns the fixed 32^3 table for 5bit. */
    bits = 5u;
    side = 1u << bits;
    if (cell >= side * side * side) return;
    shift = 8u - bits;
    half_step = 1u << (shift - 1u);
    address = uint3(cell >> (2u * bits),
        (cell >> bits) & (side - 1u), cell & (side - 1u));
    low = max(int3(address << shift) - int(half_step), int3(0));
    high = int3((address + 1u) << shift) - int(half_step) - 1;
    high = select(high, int3(255), address == side - 1u);
    // Twice the center keeps half-byte endpoints exact, including
    // the clipped first cell and the saturated final cell.
    center2 = low + high;
    best = 0u;
    distance = UINT_MAX;
    for (i = 0u; i < count; ++i) {
        delta = center2 - 2 * int3(palette[3u * i],
            palette[3u * i + 1u], palette[3u * i + 2u]);
        d = uint(delta.x * delta.x + delta.y * delta.y +
            delta.z * delta.z);
        if (d < distance) { distance = d; best = i; }
    }
    table[cell] = uchar(best);
}

/* Bucket addressing is the declared approximation, not an RGB8 rounding
 * fallback. Query sub-byte position survives through the address boundary. */
static uint lookup_rgb8_fractional(float3 query, constant uchar *palette,
    uint count, constant uchar *configuration,
    device const uchar *workspace)
{
    uint bits, shift, side;
    uint3 address;

    (void)palette; (void)count; (void)configuration;
    bits = 5u;
    shift = 8-bits;
    side = 1u << bits;
    address = min(uint3((query+float(1u << (shift-1))) /
        float(1u << shift)), uint3(side-1));
    return workspace[(address.x << (2*bits)) | (address.y << bits)
        | address.z];
}
static uint lookup_rgb8(uchar3 query, constant uchar *palette, uint count,
    constant uchar *configuration, device const uchar *workspace)
{
    return lookup_rgb8_fractional(float3(query), palette, count,
        configuration, workspace);
}
