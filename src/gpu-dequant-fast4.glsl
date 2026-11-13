/* SPDX-License-Identifier: MIT */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
layout(local_size_x = 256) in;
struct Packed {
    float x, y, z;
};
vec3 unpack3(Packed v)
{
    return vec3(v.x, v.y, v.z);
}
Packed pack3(vec3 v)
{
    return Packed(v.x, v.y, v.z);
}
bool isfinite(float v)
{
    return !isnan(v) && !isinf(v);
}
bvec3 isfinite(vec3 v)
{
    return bvec3(isfinite(v.x), isfinite(v.y), isfinite(v.z));
}
bvec4 isfinite(vec4 v)
{
    return bvec4(isfinite(v.x), isfinite(v.y), isfinite(v.z), isfinite(v.w));
}
struct DequantParams {
    uint pixel_count;
    uint width;
    uint height;
    uint ncolors;
    int similarity_bias;
};
/* vulkan-stages are compiled independently of palette lookup programs.
 * vulkan-stages: dequant_fast4
 */
layout(std430, binding = 0) buffer B0
{
    uint8_t result[];
};
layout(std430, binding = 1) readonly buffer B1
{
    uint8_t rgba[];
};
layout(std430, binding = 2) readonly buffer B2
{
    uint8_t palette[];
};
layout(std430, binding = 3) readonly buffer B3
{
    DequantParams params;
};
uint dequant_color_diff(uvec3 a, uvec3 b)
{
    int dr = int(a.x) - int(b.x);
    int dg = int(a.y) - int(b.y);
    int db = int(a.z) - int(b.z);
    return uint(dr * dr + dg * dg + db * db);
}
bool dequant_same_color(uvec3 a, uvec3 b)
{
    return all(equal(a, b));
}
uint dequant_similarity_weight(uvec3 center, uvec3 neighbor)
{
    uvec3 avg;
    uint distance;
    uint base_distance;
    uint min_diff = 0xffffffffu;
    int bias;
    if (dequant_same_color(center, neighbor)) {
        return 7u;
    }
    avg = uvec3(uint8_t((uint(center.x) + uint(neighbor.x)) >> 1),
        uint8_t((uint(center.y) + uint(neighbor.y)) >> 1),
        uint8_t((uint(center.z) + uint(neighbor.z)) >> 1));
    distance = dequant_color_diff(avg, center);
    bias = params.similarity_bias < 1 ? 1 : params.similarity_bias;
    base_distance = (distance * uint(bias) + 50u) / 100u;
    if (base_distance == 0u) {
        base_distance = 1u;
    }
    for (uint i = 0u; i < params.ncolors; ++i) {
        uvec3 p = uvec3(
            palette[i * 3u + 0u], palette[i * 3u + 1u], palette[i * 3u + 2u]);
        uint diff;
        if (dequant_same_color(p, center) || dequant_same_color(p, neighbor)) {
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
void dequant_store_zero(uint gid)
{
    result[gid * 4u + 0u] = uint8_t(0);
    result[gid * 4u + 1u] = uint8_t(0);
    result[gid * 4u + 2u] = uint8_t(0);
    result[gid * 4u + 3u] = uint8_t(0);
}
void dequant_fast4()
{
    uint gid = (gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x) * 256u
        + gl_LocalInvocationIndex;
    const ivec2 offsets[4]
        = ivec2[4](ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1), ivec2(-1, 0));
    uint x;
    uint y;
    uvec3 center_color;
    uint accum_r;
    uint accum_g;
    uint accum_b;
    uint total;
    if (gid >= params.pixel_count) {
        return;
    }
    if (rgba[gid * 4u + 3u] == 0u) {
        dequant_store_zero(gid);
        return;
    }
    x = gid % params.width;
    y = gid / params.width;
    center_color
        = uvec3(rgba[gid * 4u + 0u], rgba[gid * 4u + 1u], rgba[gid * 4u + 2u]);
    accum_r = uint(center_color.x) * 8u;
    accum_g = uint(center_color.y) * 8u;
    accum_b = uint(center_color.z) * 8u;
    total = 8u;
    for (uint n = 0u; n < 4u; ++n) {
        int nx = int(x) + offsets[n].x;
        int ny = int(y) + offsets[n].y;
        uint pos;
        uvec3 neighbor_color;
        uint weight;
        if (nx < 0 || ny < 0 || nx >= int(params.width)
            || ny >= int(params.height)) {
            continue;
        }
        pos = uint(ny) * params.width + uint(nx);
        if (rgba[pos * 4u + 3u] == 0u) {
            continue;
        }
        neighbor_color = uvec3(
            rgba[pos * 4u + 0u], rgba[pos * 4u + 1u], rgba[pos * 4u + 2u]);
        weight = dequant_similarity_weight(center_color, neighbor_color);
        if (weight == 0u) {
            continue;
        }
        accum_r += uint(neighbor_color.x) * weight;
        accum_g += uint(neighbor_color.y) * weight;
        accum_b += uint(neighbor_color.z) * weight;
        total += weight;
    }
    result[gid * 4u + 0u] = uint8_t(accum_r / total);
    result[gid * 4u + 1u] = uint8_t(accum_g / total);
    result[gid * 4u + 2u] = uint8_t(accum_b / total);
    result[gid * 4u + 3u] = uint8_t(255);
}
