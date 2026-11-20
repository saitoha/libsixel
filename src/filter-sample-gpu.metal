/* SPDX-License-Identifier: MIT */
#include <metal_stdlib>
using namespace metal;

struct SampleSolidParams {
    int width, height, clip_x, clip_y;
    uint points, rows, tolerance;
};

/* Largest channel difference from the centre along one five-pixel line;
 * 255 when the line leaves the image, as the CPU line spread decides. */
static int
sample_solid_spread(device const uchar *pixels, int width, int height,
    int x, int y, int dx, int dy)
{
    device const uchar *centre;
    device const uchar *px;
    int worst;
    int i;
    int c;

    if (x - 2 * dx < 0 || x + 2 * dx >= width ||
            y - 2 * dy < 0 || y + 2 * dy >= height)
        return 255;
    centre = pixels + ((size_t)y * (size_t)width + (size_t)x) * 3;
    worst = 0;
    for (i = -2; i <= 2; ++i) {
        px = pixels + ((size_t)(y + dy * i) * (size_t)width
                       + (size_t)(x + dx * i)) * 3;
        for (c = 0; c < 3; ++c)
            worst = max(worst, abs((int)px[c] - (int)centre[c]));
    }
    return worst;
}

/* One lane per scan point on the stride-3 grid. A solid point adds its
 * color to a 12-bit cell. Lanes of a SIMD group that share a cell add
 * once; integer sums do not depend on the order of the additions. */
kernel void sample_solid(
    device const uchar *pixels [[buffer(0)]],
    device atomic_uint *cells [[buffer(1)]],
    constant SampleSolidParams &p [[buffer(2)]],
    uint2 gid [[thread_position_in_grid]])
{
    int x;
    int y;
    uint key;
    uint3 rgb;
    bool pending;
    bool mine;
    uint lead;
    uint count;
    uint r;
    uint g;
    uint b;
    device const uchar *px;

    x = p.clip_x + 3 * (int)gid.x;
    y = p.clip_y + 3 * (int)gid.y;
    pending = false;
    key = 0xffffffffu;
    rgb = uint3(0);
    if (gid.x < p.points && gid.y < p.rows &&
            (sample_solid_spread(pixels, p.width, p.height, x, y, 1, 0)
                 <= (int)p.tolerance ||
             sample_solid_spread(pixels, p.width, p.height, x, y, 0, 1)
                 <= (int)p.tolerance)) {
        px = pixels + ((size_t)y * (size_t)p.width + (size_t)x) * 3;
        rgb = uint3(px[0], px[1], px[2]);
        key = ((rgb.x >> 4) << 8) | ((rgb.y >> 4) << 4) | (rgb.z >> 4);
        pending = true;
    }
    while (simd_any(pending)) {
        lead = simd_min(pending ? key : 0xffffffffu);
        mine = pending && key == lead;
        count = simd_sum(mine ? 1u : 0u);
        r = simd_sum(mine ? rgb.x : 0u);
        g = simd_sum(mine ? rgb.y : 0u);
        b = simd_sum(mine ? rgb.z : 0u);
        if (simd_is_first()) {
            atomic_fetch_add_explicit(&cells[lead * 5], count,
                memory_order_relaxed);
            atomic_fetch_add_explicit(&cells[lead * 5 + 1], r,
                memory_order_relaxed);
            atomic_fetch_add_explicit(&cells[lead * 5 + 2], g,
                memory_order_relaxed);
            atomic_fetch_add_explicit(&cells[lead * 5 + 3], b,
                memory_order_relaxed);
        }
        pending = pending && !mine;
    }
}
