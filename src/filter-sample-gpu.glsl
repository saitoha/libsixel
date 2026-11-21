/* SPDX-License-Identifier: MIT */
/* Accel: SAMPLE-VARY-POINTS vulkan */
/* The standalone stage uses the same source for runtime and prebuilt SPIR-V.
 * vulkan-stages: sample_solid
 */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_vote : require
#extension GL_KHR_shader_subgroup_arithmetic : require
layout(local_size_x = 512) in;
struct Params {
    int width, height, clip_x, clip_y;
    uint points, rows, tolerance;
};
layout(std430, binding = 0) readonly buffer B0 { uint8_t pixels[]; };
layout(std430, binding = 1) buffer B1 { uint cells[]; };
layout(std430, binding = 2) readonly buffer B2 { Params p; };

/* Match the CPU five-pixel line, including its out-of-frame result. */
int sample_spread(int x, int y, int dx, int dy)
{
    int worst = 0;
    if (x - 2 * dx < 0 || x + 2 * dx >= p.width ||
        y - 2 * dy < 0 || y + 2 * dy >= p.height)
        return 255;
    uint center = (uint(y) * uint(p.width) + uint(x)) * 3u;
    for (int i = -2; i <= 2; ++i) {
        uint at = (uint(y + dy * i) * uint(p.width) +
                   uint(x + dx * i)) * 3u;
        for (int c = 0; c < 3; ++c)
            worst = max(worst, abs(int(pixels[at + uint(c)]) -
                                   int(pixels[center + uint(c)])));
    }
    return worst;
}

/* One lane per stride-three point. Group equal cells within a subgroup
 * before atomic additions, as the Metal SIMD-group implementation does. */
void sample_solid()
{
    uint group = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    uint gid = group * 512u + gl_LocalInvocationIndex;
    bool pending = false;
    uint key = 0xffffffffu;
    uvec3 rgb = uvec3(0);
    if (gid < p.points * p.rows) {
        int x = p.clip_x + int(3u * (gid % p.points));
        int y = p.clip_y + int(3u * (gid / p.points));
        if (sample_spread(x, y, 1, 0) <= int(p.tolerance) ||
            sample_spread(x, y, 0, 1) <= int(p.tolerance)) {
            uint at = (uint(y) * uint(p.width) + uint(x)) * 3u;
            rgb = uvec3(uint(pixels[at]), uint(pixels[at + 1]),
                        uint(pixels[at + 2]));
            key = ((rgb.x >> 4) << 8) | ((rgb.y >> 4) << 4) |
                (rgb.z >> 4);
            pending = true;
        }
    }
    while (subgroupAny(pending)) {
        uint lead = subgroupMin(pending ? key : 0xffffffffu);
        bool mine = pending && key == lead;
        uint count = subgroupAdd(mine ? 1u : 0u);
        uint r = subgroupAdd(mine ? rgb.x : 0u);
        uint g = subgroupAdd(mine ? rgb.y : 0u);
        uint b = subgroupAdd(mine ? rgb.z : 0u);
        if (subgroupElect()) {
            atomicAdd(cells[lead * 5], count);
            atomicAdd(cells[lead * 5 + 1], r);
            atomicAdd(cells[lead * 5 + 2], g);
            atomicAdd(cells[lead * 5 + 3], b);
        }
        pending = pending && !mine;
    }
}
