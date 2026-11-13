kernel void lookup_none_apply(
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
    uint gid [[thread_position_in_grid]])
{
    uchar3 query;
    uchar3 source;
    uint best;
    uint best_dist;
    if (prepare_query(result, pixels, transparent_mask, blue_noise,
                      gradient, params, accumulation,
                      accumulation_valid_mask, accumulation_result_mask,
                      gid, query, source)) {
        return;
    }
    best = 0u;
    best_dist = 0xffffffffu;
    for (uint i = 0u; i < params.ncolors; ++i) {
        uint dist = palette_distance(palette, query, i);
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    finish_pixel(result, params, accumulation, accumulation_valid_mask,
                 accumulation_result_mask, gid, source, best);
}
