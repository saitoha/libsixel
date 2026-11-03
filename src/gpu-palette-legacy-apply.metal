kernel void sixel_gpu_palette_apply(
    device uchar *result [[buffer(0)]],
    device const uchar *pixels [[buffer(1)]],
    constant uchar *palette [[buffer(2)]],
    device const uchar *transparent_mask [[buffer(3)]],
    device const uchar *blue_noise [[buffer(4)]],
    device const uchar *gradient [[buffer(5)]],
    constant Params& params [[buffer(6)]],
    device const uchar *accumulation [[buffer(7)]],
    device const uchar *accumulation_valid_mask [[buffer(8)]],
    device uchar *accumulation_result_mask [[buffer(9)]],
    constant EytzingerEntry *eytzinger_sorted [[buffer(11)]],
    constant uint *eytzinger_rank [[buffer(10)]],
    constant EytzingerParams& eytzinger_params [[buffer(12)]],
    device const GridCell *grid [[buffer(14)]],
    device const uint *lookup_storage [[buffer(15)]],
    uint gid [[thread_position_in_grid]])
{
    uchar3 query;
    uchar3 source;
    uint best;
    uint best_dist;
    uint bits, shift, at;
    uint3 cell;
    if (prepare_query(result, pixels, transparent_mask, blue_noise,
                      gradient, params, accumulation,
                      accumulation_valid_mask, accumulation_result_mask,
                      gid, query, source)) {
        return;
    }
    if (params.lookup_mode >= 6u) {
        bits = params.lookup_mode - 1u;
        shift = 8u - bits;
        cell = min((uint3(query) + (1u << (shift - 1u))) >>
            shift, uint3((1u << bits) - 1u));
        at = (cell.x << (2u * bits)) | (cell.y << bits) | cell.z;
        best = uint(((device const uchar *)lookup_storage)[at]);
    } else if (params.lookup_mode == 5u) {
        best = fhedt_palette_lookup(palette, lookup_storage, query, params);
    } else if (params.lookup_mode == 4u) {
        best = delaunay_palette_lookup(palette, lookup_storage, query);
    } else if (params.lookup_mode == 2u) {
        best = grid_palette_lookup(palette, grid, params.ncolors, query);
    } else if (params.lookup_mode == 1u) {
        best = eytzinger_palette_lookup(palette,
                                        eytzinger_sorted,
                                        eytzinger_rank,
                                        eytzinger_params,
                                        query);
    } else {
        best = 0u;
        best_dist = 0xffffffffu;
        for (uint i = 0u; i < params.ncolors; ++i) {
            uint dist = palette_distance(palette, query, i);
            if (dist < best_dist) {
                best_dist = dist;
                best = i;
            }
        }
    }
    finish_pixel(result, params, accumulation, accumulation_valid_mask,
                 accumulation_result_mask, gid, source, best);
}
