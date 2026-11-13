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
bool isfinite(float v)
{
    return !isnan(v) && !isinf(v);
}
bvec3 isfinite(vec3 v)
{
    return bvec3(isfinite(v.x), isfinite(v.y), isfinite(v.z));
}
struct Lookup {
    uint count, colors;
};
struct Match {
    uint index;
    float distance;
};
/* Lowest-index ties and finite checks precede host publication.
 * vulkan-stages: repair_lookup
 */
layout(std430, binding = 0) readonly buffer B0
{
    Packed queries[];
};
layout(std430, binding = 1) readonly buffer B1
{
    Packed palette[];
};
layout(std430, binding = 2) buffer B2
{
    Match matches[];
};
layout(std430, binding = 3) readonly buffer B3
{
    Lookup p;
};
void repair_lookup()
{
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= p.count)
        return;
    vec3 q = unpack3(queries[gid]);
    Match best;
    best.index = 0;
    best.distance = uintBitsToFloat(0x7f800000u);
    bool invalid = !all(isfinite(q));
    for (uint i = 0; i < p.colors; ++i) {
        vec3 c = unpack3(palette[i]);
        vec3 d = q - c;
        float e = d.x * d.x + d.y * d.y + d.z * d.z;
        invalid = invalid || !all(isfinite(c)) || !isfinite(e);
        if (e < best.distance) {
            best.index = i;
            best.distance = e;
        }
    }
    if (invalid)
        best.index = 0xffffffffu;
    matches[gid] = best;
}
