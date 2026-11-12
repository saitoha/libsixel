#version 450
/* SPDX-License-Identifier: MIT */
/*
 * Builder prelude of the "vulkan.lookup-rgb8.v1" program ABI. A builder
 * program is this prelude, then a lookup policy's own GLSL, then a main()
 * that calls one builder entry the policy's plan names. The bindings are
 * the Metal builder buffers: palette at 0, the color count at 1, the plan
 * constants as words at 2 and the policy's writable workspace at
 * SIXEL_LOOKUP_WORKSPACE_BINDING (3). Specialization constants 0..2 carry
 * the pass's local thread dimensions.
 */
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(std430, set = 0, binding = 0) readonly buffer LookupPalette {
    uint8_t lookup_palette[];
};
layout(std430, set = 0, binding = 1) readonly buffer LookupCount {
    uint lookup_count;
};
layout(std430, set = 0, binding = 2) readonly buffer LookupConstants {
    uint lookup_constants[];
};
#define SIXEL_LOOKUP_WORKSPACE_BINDING 3
#define SIXEL_LOOKUP_BUILDER 1
