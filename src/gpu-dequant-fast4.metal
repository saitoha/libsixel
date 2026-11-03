struct DequantParams {
    uint pixel_count;
    uint width;
    uint height;
    uint ncolors;
    int similarity_bias;
};
static uint dequant_color_diff(uchar3 a, uchar3 b)
{
    int dr = int(a.x) - int(b.x);
    int dg = int(a.y) - int(b.y);
    int db = int(a.z) - int(b.z);
    return uint(dr * dr + dg * dg + db * db);
}
static bool dequant_same_color(uchar3 a, uchar3 b)
{
    return all(a == b);
}
static uint dequant_similarity_weight(device const uchar *palette,
                                      constant DequantParams& params,
                                      uchar3 center,
                                      uchar3 neighbor)
{
    uchar3 avg;
    uint distance;
    uint base_distance;
    uint min_diff = 0xffffffffu;
    int bias;
    if (dequant_same_color(center, neighbor)) {
        return 7u;
    }
    avg = uchar3(uchar((uint(center.x) + uint(neighbor.x)) >> 1),
                 uchar((uint(center.y) + uint(neighbor.y)) >> 1),
                 uchar((uint(center.z) + uint(neighbor.z)) >> 1));
    distance = dequant_color_diff(avg, center);
    bias = params.similarity_bias < 1 ? 1 : params.similarity_bias;
    base_distance = (distance * uint(bias) + 50u) / 100u;
    if (base_distance == 0u) {
        base_distance = 1u;
    }
    for (uint i = 0u; i < params.ncolors; ++i) {
        uchar3 p = uchar3(palette[i * 3u + 0u],
                          palette[i * 3u + 1u],
                          palette[i * 3u + 2u]);
        uint diff;
        if (dequant_same_color(p, center) ||
            dequant_same_color(p, neighbor)) {
            continue;
        }
        diff = dequant_color_diff(avg, p);
        if (diff < min_diff) {
            min_diff = diff;
        }
    }
    if (min_diff == 0xffffffffu) {
        min_diff = base_distance * 2u;
    }
    if (min_diff >= base_distance * 2u) {
        return 5u;
    }
    if (min_diff >= base_distance) {
        return 8u;
    }
    if (min_diff * 6u >= base_distance * 5u) {
        return 7u;
    }
    if (min_diff * 4u >= base_distance * 3u) {
        return 7u;
    }
    if (min_diff * 3u >= base_distance * 2u) {
        return 5u;
    }
    if (min_diff * 5u >= base_distance * 3u) {
        return 7u;
    }
    if (min_diff * 2u >= base_distance) {
        return 4u;
    }
    if (min_diff * 3u >= base_distance) {
        return 2u;
    }
    return 0u;
}
static void dequant_store_zero(device uchar *result, uint gid)
{
    result[gid * 4u + 0u] = uchar(0);
    result[gid * 4u + 1u] = uchar(0);
    result[gid * 4u + 2u] = uchar(0);
    result[gid * 4u + 3u] = uchar(0);
}
kernel void sixel_gpu_dequant_fast4_rgba(
    device uchar *result [[buffer(0)]],
    device const uchar *rgba [[buffer(1)]],
    device const uchar *palette [[buffer(2)]],
    constant DequantParams& params [[buffer(3)]],
    uint gid [[thread_position_in_grid]])
{
    const int2 offsets[4] = {
        int2(-1, -1), int2(0, -1), int2(1, -1), int2(-1, 0)
    };
    uint x;
    uint y;
    uchar3 center_color;
    uint accum_r;
    uint accum_g;
    uint accum_b;
    uint total;
    if (gid >= params.pixel_count) {
        return;
    }
    if (rgba[gid * 4u + 3u] == 0u) {
        dequant_store_zero(result, gid);
        return;
    }
    x = gid % params.width;
    y = gid / params.width;
    center_color = uchar3(rgba[gid * 4u + 0u], rgba[gid * 4u + 1u],
                          rgba[gid * 4u + 2u]);
    accum_r = uint(center_color.x) * 8u;
    accum_g = uint(center_color.y) * 8u;
    accum_b = uint(center_color.z) * 8u;
    total = 8u;
    for (uint n = 0u; n < 4u; ++n) {
        int nx = int(x) + offsets[n].x;
        int ny = int(y) + offsets[n].y;
        uint pos;
        uchar3 neighbor_color;
        uint weight;
        if (nx < 0 || ny < 0 || nx >= int(params.width) ||
            ny >= int(params.height)) {
            continue;
        }
        pos = uint(ny) * params.width + uint(nx);
        if (rgba[pos * 4u + 3u] == 0u) {
            continue;
        }
        neighbor_color = uchar3(rgba[pos * 4u + 0u],
                                rgba[pos * 4u + 1u],
                                rgba[pos * 4u + 2u]);
        weight = dequant_similarity_weight(palette,
                                           params,
                                           center_color,
                                           neighbor_color);
        if (weight == 0u) {
            continue;
        }
        accum_r += uint(neighbor_color.x) * weight;
        accum_g += uint(neighbor_color.y) * weight;
        accum_b += uint(neighbor_color.z) * weight;
        total += weight;
    }
    result[gid * 4u + 0u] = uchar(accum_r / total);
    result[gid * 4u + 1u] = uchar(accum_g / total);
    result[gid * 4u + 2u] = uchar(accum_b / total);
    result[gid * 4u + 3u] = uchar(255);
}
