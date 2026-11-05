/*
 * Shared per-pixel stages.  prepare_query returns true when the pixel is
 * finished without a palette search (out of range, transparent, or kept by
 * 6delta), after writing any result.  Otherwise it yields the search query
 * (after dither noise) and the selected delta comparison sample.
 */
/* Build exactly the byte query consumed by lookup, including truncation
 * and clipping. The default target gate can skip this work entirely. */
static uchar3 positional_query(uchar3 source,
    device const uchar *blue_noise, device const uchar *gradient,
    constant Params& params, uint x, uint y, uint gid)
{
    uchar q[3] = { source.x, source.y, source.z };
    if (params.mode == 1u) {
        float weight = gradient_weight(gradient, params, x, y, gid);
        for (uint d = 0u; d < 3u; ++d) {
            int val = int(q[d]) + int(tri_noise(blue_noise,
                                                params,
                                                int(x),
                                                int(y),
                                                int(d)) * weight * 32.0f);
            val = val < 0 ? 0 : (val > 255 ? 255 : val);
            q[d] = uchar(val);
        }
    }
    // Match CPU unsigned phases and truncate the bias before addition.
    if (params.mode == 2u || params.mode == 3u) {
        for (uint d = 0u; d < 3u; ++d) {
            uint phase;
            float noise;
            if (params.mode == 2u) {
                phase = (x + d * 67u + y * 236u) * 119u;
                noise = float(phase & 255u) / 128.0f - 1.0f;
            } else {
                phase = ((x + d * 29u) ^ (y * 149u)) * 1234u;
                noise = float(phase & 511u) / 256.0f - 1.0f;
            }
            int val = int(q[d]) +
                int((noise * params.positional_strength) * 32.0f);
            q[d] = uchar(clamp(val, 0, 255));
        }
    }
    return uchar3(q[0], q[1], q[2]);
}
static bool prepare_query(
    device uchar *result,
    device const uchar *pixels,
    device const uchar *transparent_mask,
    device const uchar *blue_noise,
    device const uchar *gradient,
    constant Params& params,
    device const uchar *accumulation,
    device const uchar *accumulation_valid_mask,
    device uchar *accumulation_result_mask,
    uint gid,
    thread uchar3& query,
    thread uchar3& comparison)
{
    uint x;
    uint y;
    uint local_threshold, cell;
    uint retained_index;
    bool within;
    int dr;
    int dg;
    int db;
    query = uchar3(0);
    comparison = uchar3(0);
    if (gid >= params.pixel_count) {
        return true;
    }
    if (params.has_transparent != 0u && gid < params.transparent_size &&
        transparent_mask[gid] != 0u) {
        result[gid] = uchar(params.transparent_keycolor);
        if (gid < params.accumulation_result_mask_size) {
            accumulation_result_mask[gid] = uchar(1);
        }
        return true;
    }
    x = gid % params.width;
    y = gid / params.width;
    retained_index = params.accumulation_origin +
        y * params.accumulation_stride + x;
    comparison = uchar3(pixels[gid * 3u], pixels[gid * 3u + 1u],
        pixels[gid * 3u + 2u]);
    if (params.sixdelta_compare != 0u) {
        query = positional_query(comparison, blue_noise, gradient,
            params, x, y, gid);
        comparison = query;
    }
    if (params.has_6delta != 0u &&
        gid < params.accumulation_pixel_count &&
        (gid >= params.accumulation_valid_mask_size ||
         accumulation_valid_mask[retained_index] != 0u)) {
        dr = int(comparison.x) - int(accumulation[retained_index * 3u + 0u]);
        dg = int(comparison.y) - int(accumulation[retained_index * 3u + 1u]);
        db = int(comparison.z) - int(accumulation[retained_index * 3u + 2u]);
        dr = dr < 0 ? -dr : dr;
        dg = dg < 0 ? -dg : dg;
        db = db < 0 ? -db : db;
        local_threshold = params.sixdelta_threshold;
        if (params.residual_columns != 0u) {
            if (params.residual_tiled != 0u)
                cell = (y & 63u) * 64u + (x & 63u);
            else
                cell = (((y + 1u) * params.residual_rows - 1u) /
                    params.height) * params.residual_columns +
                    ((x + 1u) * params.residual_columns - 1u) /
                    params.width;
            if ((params.residual_cells[cell / 32u] &
                    (1u << (cell % 32u))) != 0u)
                local_threshold = min(local_threshold,
                    params.residual_threshold);
        }
        within = uint(dr) <= local_threshold &&
            uint(dg) <= local_threshold && uint(db) <= local_threshold;
        if (params.sixdelta_metric == 2u)
            within = uint(2*dr*dr + 4*dg*dg + db*db) <=
                7u * local_threshold * local_threshold;
        if (within) {
            result[gid] = uchar(params.accumulation_keycolor);
            if (gid < params.accumulation_result_mask_size) {
                accumulation_result_mask[gid] =
                    uchar((dr | dg | db) != 0 ? 3 : 1);
            }
            return true;
        }
    }
    if (params.sixdelta_compare == 0u) {
        query = positional_query(comparison, blue_noise, gradient,
            params, x, y, gid);
    }
    return false;
}
static void finish_pixel(
    device uchar *result,
    constant Params& params,
    device const uchar *accumulation,
    device const uchar *accumulation_valid_mask,
    device uchar *accumulation_result_mask,
    uint gid,
    uchar3 comparison,
    uint best)
{
    uchar3 retained;
    uint retained_index = params.accumulation_origin +
        (gid / params.width) * params.accumulation_stride + gid % params.width;
    if (params.has_6delta != 0u &&
        gid < params.accumulation_pixel_count &&
        (gid >= params.accumulation_valid_mask_size ||
         accumulation_valid_mask[retained_index] != 0u)) {
        retained = uchar3(accumulation[retained_index * 3u + 0u],
                          accumulation[retained_index * 3u + 1u],
                          accumulation[retained_index * 3u + 2u]);
        if (color_distance(comparison, retained) <=
            palette_distance(params.sixdelta_palette, comparison, best)) {
            result[gid] = uchar(params.accumulation_keycolor);
            if (gid < params.accumulation_result_mask_size) {
                accumulation_result_mask[gid] = uchar(1);
            }
            return;
        }
    }
    result[gid] = uchar(best);
}
