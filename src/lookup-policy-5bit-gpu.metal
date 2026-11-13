/* SPDX-License-Identifier: MIT */
#include <metal_stdlib>
using namespace metal;
kernel void lookup_5bit_build(device uchar *table [[buffer(0)]],
    constant uchar *palette [[buffer(1)]],
    constant uint &count [[buffer(2)]],
    uint cell [[thread_position_in_grid]])
{
    uint bits, side, shift, half_step, best, distance, i, d;
    uint3 address;
    int3 low, high, center2, delta;
    /* The 5bit table has its own builder and fixed geometry. */
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
