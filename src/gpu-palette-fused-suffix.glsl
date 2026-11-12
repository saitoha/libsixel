/* SPDX-License-Identifier: MIT */
/*
 * Closes the fused palette-apply kernel begun by
 * gpu-palette-fused-prefix.glsl around the lookup policy's lookup_rgb8(),
 * as gpu-palette-fused-suffix.metal does. An index outside the palette
 * marks the run invalid instead of being written.
 */
void main()
{
    uvec3 query;
    uvec3 source;
    uint best;
    uint local = gl_GlobalInvocationID.y * range.pitch +
        gl_GlobalInvocationID.x;
    uint gid;

    if (local >= range.count) {
        return;
    }
    gid = range.first + local;
    if (prepare_query(gid, query, source)) {
        return;
    }
    best = lookup_rgb8(query, params.ncolors);
    if (best >= params.ncolors) {
        atomicOr(lookup_invalid, 1u);
        return;
    }
    finish_pixel(gid, source, best);
}
