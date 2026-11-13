/* SPDX-License-Identifier: MIT */
/* x-dither query for legacy lookup executors. */
static uchar3 positional_query(uchar3 source,
    device const uchar *blue_noise, device const uchar *gradient,
    constant Params& params, uint x, uint y, uint gid)
{
    uchar q[3] = { source.x, source.y, source.z };
    uint d, phase;
    float noise;
    int value;

    /* Preserve unsigned phases and truncate the bias before addition. */
    for (d = 0u; d < 3u; ++d) {
        phase = ((x + d * 29u) ^ (y * 149u)) * 1234u;
        noise = float(phase & 511u) / 256.0f - 1.0f;
        value = int(q[d]) + int((noise * params.positional_strength) * 32.0f);
        q[d] = uchar(clamp(value, 0, 255));
    }
    return uchar3(q[0], q[1], q[2]);
}
