/* SPDX-License-Identifier: MIT */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
layout(local_size_x = 256) in;
struct Params {
    uint width, height, x, y, stride, count, record_paint;
    uint8_t palette[768];
};
layout(std430, binding = 0) readonly buffer B0
{
    uint8_t rgb[];
};
layout(std430, binding = 1) readonly buffer B1
{
    uint8_t valid[];
};
layout(std430, binding = 2) buffer B2
{
    uint8_t next_rgb[];
};
layout(std430, binding = 3) buffer B3
{
    uint8_t next_valid[];
};
layout(std430, binding = 4) readonly buffer B4
{
    uint8_t indexes[];
};
layout(std430, binding = 5) readonly buffer B5
{
    uint8_t skip[];
};
layout(std430, binding = 6) buffer B6
{
    uint8_t paint[];
};
layout(std430, binding = 7) buffer B7
{
    uvec2 stats[];
};
layout(std430, binding = 8) readonly buffer B8
{
    Params p;
};
shared uvec2 reductions[256];
/* The inactive canvas is synchronized before dispatch. Every lane then
 * copies a keep exactly or expands an emitted palette color at its origin.
 * vulkan-stages: retained_expand
 */
void retained_expand()
{
    uint group = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    uint lane = gl_LocalInvocationIndex;
    uint gid = group * 256u + lane;
    uvec2 result = uvec2(0);
    if (gid < p.count) {
        uint target = (p.y + gid / p.width) * p.stride + p.x + gid % p.width;
        uint state = uint(skip[gid]);
        uint color = state == 0 ? uint(indexes[gid]) : 0;
        for (uint c = 0; c < 3; ++c)
            next_rgb[target * 3 + c]
                = state == 0 ? p.palette[color * 3 + c] : rgb[target * 3 + c];
        next_valid[target] = state == 0 ? uint8_t(1) : valid[target];
        result = uvec2(uint(state == 0), uint(state == 3));
        if (p.record_paint != 0)
            paint[gid] = uint8_t(state == 0);
    }
    reductions[lane] = result;
    barrier();
    for (uint step = 128; step != 0; step >>= 1) {
        if (lane < step) {
            reductions[lane].x += reductions[lane + step].x;
            reductions[lane].y |= reductions[lane + step].y;
        }
        barrier();
    }
    if (lane == 0 && group < (p.count + 255u) / 256u)
        stats[group] = reductions[0];
}
