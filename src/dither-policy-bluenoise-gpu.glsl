/* SPDX-License-Identifier: MIT */
/*
 * Closes the fused palette-apply kernel begun by
 * dither-policy-bluenoise-gpu-prefix.glsl around the lookup policy's lookup_rgb8(),
 * as dither-policy-bluenoise-gpu.metal does. An index outside the palette
 * marks the run invalid instead of being written.
 */
/* Batch programs consume two spatial queries per lane. Point programs
 * retain their linear 64-thread dispatch. The host validates the shape. */
#if defined(SIXEL_LOOKUP_BATCH)
layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
#else
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
#endif

#if defined(SIXEL_LOOKUP_BATCH)
void main()
{
    vec4 queries[2];
    uvec3 query, source[2];
    uvec2 indexes;
    uint gid[2];
    bool done[2];
    uint tile, columns, h, pos, x, y, local;

    tile = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    columns = (params.width + 7u) / 8u;
    for (h = 0u; h < 2u; ++h) {
        pos = gl_LocalInvocationID.x + h * 32u;
        x = (tile % columns) * 8u + pos % 8u;
        y = (tile / columns) * 8u + pos / 8u;
        queries[h] = vec4(0.0);
        done[h] = true;
        if (x < params.width && y < range.count / params.width) {
            local = y * params.width + x;
            gid[h] = range.first + local;
            done[h] = prepare_query(gid[h], query, source[h]);
            if (!done[h]) queries[h] = vec4(vec3(query), 1.0);
        }
    }
    /* Every lane participates, even when both pixels are inactive. */
    lookup_rgb8_batch(queries, params.ncolors, indexes);
    for (h = 0u; h < 2u; ++h) {
        if (done[h]) continue;
        if (indexes[h] >= params.ncolors) {
            atomicOr(lookup_invalid[range.flag], 1u);
        } else {
            finish_pixel(gid[h], source[h], indexes[h]);
        }
    }
}
#else
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
        atomicOr(lookup_invalid[range.flag], 1u);
        return;
    }
    finish_pixel(gid, source, best);
}

#endif
