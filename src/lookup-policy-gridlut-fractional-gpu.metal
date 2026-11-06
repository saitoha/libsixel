#include <metal_stdlib>
using namespace metal;

struct GridlutCell {
    uchar count;
    uchar indexes[64];
};

/* A cell includes fractional coordinates above its last integer. Using the
 * closed upper face is conservative; a candidate overflow requests a scan. */
kernel void gridlut_build(constant uchar *palette [[buffer(0)]],
    constant uint &ncolors [[buffer(1)]],
    device GridlutCell *grid [[buffer(3)]],
    uint cell [[thread_position_in_grid]])
{
    int3 low, high, color, gap;
    uint bound, far, near, count, i;

    if (cell >= 4096u) return;
    low = int3(cell >> 8u, (cell >> 4u) & 15u, cell & 15u) * 16;
    high = min(low + 16, int3(255));
    bound = UINT_MAX;
    for (i = 0; i < ncolors; ++i) {
        color = int3(palette[3*i], palette[3*i+1], palette[3*i+2]);
        gap = max(color-low, high-color);
        far = uint(gap.x*gap.x + gap.y*gap.y + gap.z*gap.z);
        bound = min(bound, far);
    }
    count = 0;
    for (i = 0; i < ncolors; ++i) {
        color = int3(palette[3*i], palette[3*i+1], palette[3*i+2]);
        gap = max(max(low-color, color-high), int3(0));
        near = uint(gap.x*gap.x + gap.y*gap.y + gap.z*gap.z);
        if (near <= bound) {
            if (count < 64) grid[cell].indexes[count] = uchar(i);
            ++count;
        }
    }
    grid[cell].count = count <= 64 ? uchar(count) : uchar(0);
}

static uint lookup_rgb8_fractional(float3 query, constant uchar *palette,
    uint ncolors, constant uchar *configuration,
    device const uchar *workspace)
{
    device const GridlutCell *grid;
    uint3 address;
    uint cell, count, i, index, best;
    float minimum, distance;
    float3 gap;

    (void)configuration;
    grid = (device const GridlutCell *)workspace;
    address = uint3(query) >> 4;
    cell = (address.x << 8) | (address.y << 4) | address.z;
    count = grid[cell].count;
    if (count == 1) return grid[cell].indexes[0];
    minimum = INFINITY;
    best = 0;
    for (i = 0; i < (count ? count : ncolors); ++i) {
        index = count ? uint(grid[cell].indexes[i]) : i;
        gap = query - float3(palette[3*index], palette[3*index+1],
            palette[3*index+2]);
        distance = dot(gap, gap);
        if (distance < minimum) { minimum = distance; best = index; }
    }
    return best;
}

static uint lookup_rgb8(uchar3 query, constant uchar *palette, uint ncolors,
    constant uchar *configuration, device const uchar *workspace)
{
    return lookup_rgb8_fractional(float3(query), palette, ncolors,
        configuration, workspace);
}
