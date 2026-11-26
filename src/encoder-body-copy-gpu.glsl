/* SPDX-License-Identifier: MIT */
/* Accel: BODY-SPELL vulkan */
#version 450
layout(local_size_x = 256) in;

/* The spelled length is produced on the GPU, so a shader copies exactly
 * that many words into cached host memory in the same submission. */
/*
 * vulkan-stages: body_copy_bytes body_resident_copy
 */
layout(std430, binding = 0) readonly buffer B0 { uint source[]; };
layout(std430, binding = 1) writeonly buffer B1 { uint target[]; };
layout(std430, binding = 2) readonly buffer B2 { uint band_bytes[]; };
layout(std430, binding = 3) readonly buffer B3 {
    uint width, words, ncolors, bands;
    uint selectors, limited, nodes, group_limit;
} e;

void body_copy_bytes()
{
    uint group = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    uint index = group * 256u + gl_LocalInvocationIndex;
    uint limit = (band_bytes[e.bands] + 3u) / 4u;
    if (index < limit) target[index] = source[index];
}


/* Accel: BODY-RESIDENT vulkan */
void body_resident_copy()
{
    uint index = gl_GlobalInvocationID.x;
    uint size = band_bytes[e.bands];
    if (index == 0u)
        target[0] = size;
    if (index < (size + 3u) / 4u)
        target[index + 1u] = source[index];
}
