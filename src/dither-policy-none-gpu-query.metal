/* SPDX-License-Identifier: MIT */
/* none query for legacy lookup executors. */
static uchar3 positional_query(uchar3 source,
    device const uchar *blue_noise, device const uchar *gradient,
    constant Params& params, uint x, uint y, uint gid)
{
    return source;
}
