/* SPDX-License-Identifier: MIT */
/* bluenoise query for legacy lookup executors. */
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
static uchar3 positional_query(uchar3 source,
    device const uchar *blue_noise, device const uchar *gradient,
    constant Params& params, uint x, uint y, uint gid)
{
    uchar q[3] = { source.x, source.y, source.z };
    uint d;
    int value;
    float weight;

    weight = gradient_weight(gradient, params, x, y, gid);
    for (d = 0u; d < 3u; ++d) {
        value = int(q[d]) + int(tri_noise(blue_noise, params,
            int(x), int(y), int(d)) * weight * 32.0f);
        q[d] = uchar(clamp(value, 0, 255));
    }
    return uchar3(q[0], q[1], q[2]);
}
