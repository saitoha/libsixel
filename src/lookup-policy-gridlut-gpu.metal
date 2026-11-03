struct GridCell {
    uchar count;
    uchar indexes[64];
};
kernel void sixel_gpu_grid_build(
    constant uchar *palette [[buffer(2)]],
    constant Params& params [[buffer(6)]],
    device GridCell *grid [[buffer(14)]],
    uint cell [[thread_position_in_grid]])
{
    int3 low;
    int3 high;
    int3 color;
    int3 gap;
    uint bound;
    uint far;
    uint near;
    uint count;
    uint i;
    if (cell >= 4096u) {
        return;
    }
    low = int3(cell >> 8u, (cell >> 4u) & 15u, cell & 15u) * 16;
    high = low + 15;
    bound = 0xffffffffu;
    for (i = 0u; i < params.ncolors; ++i) {
        color = int3(palette[i * 3u], palette[i * 3u + 1u],
                     palette[i * 3u + 2u]);
        gap = max(color - low, high - color);
        far = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        bound = min(bound, far);
    }
    count = 0u;
    for (i = 0u; i < params.ncolors; ++i) {
        color = int3(palette[i * 3u], palette[i * 3u + 1u],
                     palette[i * 3u + 2u]);
        gap = max(max(low - color, color - high), int3(0));
        near = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        if (near <= bound) {
            if (count < 64u) {
                grid[cell].indexes[count] = uchar(i);
            }
            ++count;
        }
    }
    grid[cell].count = count <= 64u ? uchar(count) : uchar(0);
}
static uint grid_palette_lookup(constant uchar *palette,
                                device const GridCell *grid,
                                uint ncolors, uchar3 q)
{
    uint cell;
    uint count;
    uint best;
    uint best_dist;
    uint index;
    uint dist;
    uint i;
    cell = (uint(q.x >> 4) << 8) | (uint(q.y >> 4) << 4) | (q.z >> 4);
    count = grid[cell].count;
    if (count == 1u) {
        return grid[cell].indexes[0];
    }
    best = 0u;
    best_dist = 0xffffffffu;
    for (i = 0u; i < (count != 0u ? count : ncolors); ++i) {
        index = count != 0u ? uint(grid[cell].indexes[i]) : i;
        dist = palette_distance(palette, q, index);
        if (dist < best_dist) {
            best_dist = dist;
            best = index;
        }
    }
    return best;
}
