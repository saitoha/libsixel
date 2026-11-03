static float bn_sample(device const uchar *blue_noise, int x, int y)
{
    uint xx = uint(x & 63);
    uint yy = uint(y & 63);
    return float(blue_noise[yy * 64u + xx]) / 127.5f - 1.0f;
}
static float tri_noise(device const uchar *blue_noise,
                       constant Params& params,
                       int x,
                       int y,
                       int c)
{
    const int offset_x[3] = { 17, 34, 51 };
    const int offset_y[3] = { 31, 62, 93 };
    int channel_x = 0;
    int channel_y = 0;
    float sample_u;
    float sample_v;
    if (params.bluenoise_channel_rgb != 0u && c >= 0 && c < 3) {
        channel_x = offset_x[c];
        channel_y = offset_y[c];
    }
    sample_u = (bn_sample(blue_noise,
                          x + params.bluenoise_phase_x + channel_x,
                          y + params.bluenoise_phase_y + channel_y)
                + 1.0f) * 0.5f;
    sample_v = (bn_sample(blue_noise,
                          x + params.bluenoise_phase_x + channel_x + 13,
                          y + params.bluenoise_phase_y + channel_y + 29)
                + 1.0f) * 0.5f;
    return ((sample_u + sample_v) - 1.0f) * params.bluenoise_strength;
}
static float gradient_weight(device const uchar *gradient,
                             constant Params& params,
                             uint x,
                             uint y,
                             uint index)
{
    float normalized;
    float attenuated;
    if (params.bluenoise_gradient_factor <= 0.0f) {
        return 1.0f;
    }
    if (params.gradient_width == 0u || params.gradient_height == 0u ||
        x >= params.gradient_width || y >= params.gradient_height ||
        index >= params.gradient_size) {
        return 1.0f;
    }
    normalized = float(gradient[index]) / 255.0f;
    if (normalized <= 0.0f) {
        return 1.0f;
    }
    if (normalized >= 1.0f) {
        return 0.0f;
    }
    attenuated = pow(normalized, params.bluenoise_gradient_factor);
    attenuated = clamp(attenuated, 0.0f, 1.0f);
    return 1.0f - attenuated;
}
static uint palette_distance(
    constant uchar *palette,
    uchar3 q,
    uint palette_index)
{
    int dr = int(q.x) - int(palette[palette_index * 3u + 0u]);
    int dg = int(q.y) - int(palette[palette_index * 3u + 1u]);
    int db = int(q.z) - int(palette[palette_index * 3u + 2u]);
    return uint(dr * dr + dg * dg + db * db);
}
static uint color_distance(uchar3 left, uchar3 right)
{
    int dr = int(left.x) - int(right.x);
    int dg = int(left.y) - int(right.y);
    int db = int(left.z) - int(right.z);
    return uint(dr * dr + dg * dg + db * db);
}
static void update_palette_best(
    constant uchar *palette,
    uchar3 q,
    uint palette_index,
    thread uint& best,
    thread uint& best_dist)
{
    uint dist = palette_distance(palette, q, palette_index);
    if (dist < best_dist) {
        best_dist = dist;
        best = palette_index;
    }
}
/* Exact fixed RGB-cell candidates. Overflow keeps exhaustive lookup. */
