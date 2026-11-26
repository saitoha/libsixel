/* SPDX-License-Identifier: MIT */
/* Accel: BODY-COMPOSE vulkan */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
layout(local_size_x = 256) in;
/* Mirrors sixel_body_gpu_params_t in encoder-body-gpu-vulkan.c. */
struct Params {
    uint width, height, encoded_width, encoded_height;
    uint offset_left, offset_top, ncolors;
    int keycolor;
    uint words, bands, has_mask, write_pass;
};
layout(std430, binding = 0) buffer B0
{
    uint occupancy[];
};
layout(std430, binding = 1) buffer B1
{
    uint counts[];
};
layout(std430, binding = 2) buffer B2
{
    uint widths[];
};
layout(std430, binding = 3) buffer B3
{
    uint node_first[];
};
layout(std430, binding = 4) buffer B4
{
    uint map_first[];
};
layout(std430, binding = 5) buffer B5
{
    uvec4 nodes[];
};
layout(std430, binding = 6) buffer B6
{
    uint8_t map[];
};
layout(std430, binding = 7) readonly buffer B7
{
    uint8_t indexes[];
};
layout(std430, binding = 8) readonly buffer B8
{
    uint8_t mask[];
};
layout(std430, binding = 9) buffer B9
{
    Params p;
};
layout(std430, binding = 10) buffer B10
{
    uint first_seen[];
};

uint body_invocation()
{
    uint group = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    return group * 256u + gl_LocalInvocationIndex;
}

/* Both stages declare the same eleven bindings so one layout serves the
 * pair; the same algorithm as encoder-body-gpu-metal.m.
 * vulkan-stages: body_classify body_spans
 * vulkan-stages: body_resident_local body_resident_prefix body_resident_offsets
 */

/* Mark each painted source pixel in its (band, register) column bitmap. */
void body_classify()
{
    uint i = body_invocation();
    if (i >= p.width * p.height) return;
    if (p.has_mask != 0u && uint(mask[i]) != 0u) return;
    uint c = uint(indexes[i]);
    if (c >= p.ncolors || int(c) == p.keycolor) return;
    uint x = i % p.width + p.offset_left;
    uint band = (i / p.width + p.offset_top) / 6u;
    atomicOr(occupancy[(band * p.ncolors + c) * p.words + x / 32u],
        1u << (x % 32u));
}

uint body_next(uint base, uint limit, uint from)
{
    uint w = from / 32u;
    if (w >= p.words) return limit;
    uint bits = occupancy[base + w] & (0xffffffffu << (from % 32u));
    while (bits == 0u) {
        if (++w >= p.words) return limit;
        bits = occupancy[base + w];
    }
    return min(w * 32u + uint(findLSB(bits)), limit);
}

/* One invocation per (band, register). The count pass sizes the spans and
 * their column values; the write pass stores them at the prefix offsets,
 * together with the register's first row-major position in the band. */
void body_spans()
{
    uint t = body_invocation();
    if (t >= p.bands * p.ncolors) return;
    uint base = t * p.words;
    uint band = t / p.ncolors;
    uint c = t % p.ncolors;
    uint limit = p.encoded_width;
    uint n = 0u;
    uint used = 0u;
    uint first = 0xffffffffu;
    bool write_pass = p.write_pass != 0u;
    uint node_base = write_pass ? node_first[t] : 0u;
    uint map_base = write_pass ? map_first[t] : 0u;
    uint sx = body_next(base, limit, 0u);
    while (sx < limit) {
        uint mx = sx + 1u;
        uint nx = body_next(base, limit, mx);
        while (nx < limit && nx - mx <= 9u) {
            mx = nx + 1u;
            nx = body_next(base, limit, mx);
        }
        if (write_pass) {
            nodes[node_base + n] = uvec4(c, sx, mx, map_base + used);
            for (uint x = sx; x < mx; ++x) {
                uint v = 0u;
                int sxp = int(x) - int(p.offset_left);
                if (sxp >= 0 && uint(sxp) < p.width) {
                    for (uint r = 0u; r < 6u; ++r) {
                        int syp = int(band * 6u + r) - int(p.offset_top);
                        if (syp < 0 || uint(syp) >= p.height) continue;
                        uint i = uint(syp) * p.width + uint(sxp);
                        if (uint(indexes[i]) == c &&
                                (p.has_mask == 0u || uint(mask[i]) == 0u))
                            v |= 1u << r;
                    }
                }
                if (v != 0u)
                    first = min(first,
                        uint(findLSB(v)) * p.encoded_width + x);
                map[map_base + used + (x - sx)] = uint8_t(v);
            }
        }
        used += mx - sx;
        ++n;
        sx = nx;
    }
    if (!write_pass) {
        counts[t] = n;
        widths[t] = used;
    } else {
        first_seen[t] = first;
    }
}


/* Accel: BODY-RESIDENT vulkan */
layout(std430, binding = 11) buffer BandFirst { uint band_first[]; };
layout(std430, binding = 12) buffer MapBands { uint map_bands[]; };
layout(std430, binding = 13) buffer EmitShape {
    uint width, words, ncolors, bands;
    uint selectors, limited, nodes, group_limit;
} emit;
layout(std430, binding = 14) buffer Indirect { uint groups[]; };
layout(std430, binding = 15) writeonly buffer Registers { uint registers[]; };

/* Scan registers within each band and then scan the short band totals. */
void body_resident_local()
{
    uint b = body_invocation();
    if (b >= p.bands)
        return;
    uint n = 0u, m = 0u;
    for (uint i = b * p.ncolors; i < (b + 1u) * p.ncolors; ++i) {
        node_first[i] = n;
        map_first[i] = m;
        n += counts[i];
        m += widths[i];
    }
    band_first[b] = n;
    map_bands[b] = m;
}

void body_resident_prefix()
{
    if (body_invocation() != 0u)
        return;
    uint n = 0u, m = 0u;
    for (uint b = 0u; b < p.bands; ++b) {
        uint count = band_first[b], length = map_bands[b];
        band_first[b] = n;
        map_bands[b] = m;
        n += count;
        m += length;
    }
    band_first[p.bands] = n;
    emit.nodes = n;
    groups[0] = max(1u, (n + 255u) / 256u);
    groups[1] = groups[2] = 1u;
    p.write_pass = 1u;
}

void body_resident_offsets()
{
    uint i = body_invocation();
    if (i >= p.bands * p.ncolors)
        return;
    uint b = i / p.ncolors;
    node_first[i] += band_first[b];
    map_first[i] += map_bands[b];
    if (i < p.ncolors)
        registers[i] = i;
}
