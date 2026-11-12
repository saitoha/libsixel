#version 450
/* SPDX-License-Identifier: MIT */
/*
 * Jarvis, Judice and Ninke GPU diffusion as a consumer of the
 * "vulkan.lookup-rgb8.v1" program ABI: the declarations a lookup policy's
 * GLSL needs. The policy's chunks follow this prefix and supply
 * lookup_rgb8_fractional(); dither-policy-jajuni-gpu.glsl closes the program.
 * The buffer indexes follow dither-policy-jajuni-gpu.metal: the palette at 1,
 * the plan constants as words at 12 and the policy's workspace at 13.
 */
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 1) readonly buffer LookupPalette {
    uint8_t lookup_palette[];
};
layout(std430, set = 0, binding = 12) readonly buffer LookupConstants {
    uint lookup_constants[];
};
#define SIXEL_LOOKUP_WORKSPACE_BINDING 13
