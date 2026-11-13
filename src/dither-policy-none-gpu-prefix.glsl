/* none owns this byte-query construction and pixel execution. */
#version 450
/* SPDX-License-Identifier: MIT */
/*
 * Palette-apply consumer of the "vulkan.lookup-rgb8.v1" program ABI: the
 * prefix of the fused per-pixel kernel. prepare_query()/finish_pixel()
 * mirror dither-policy-none-gpu-prefix.metal so that positional dither,
 * transparency and 6delta keeps produce the same indexes as the Metal and
 * CPU paths. A lookup policy's own GLSL follows this prefix and supplies
 * lookup_rgb8() or a tile collective; the suffix closes the kernel. This
 * component never interprets the policy, its constants or its workspace.
 *
 * Bindings 0..9 follow the Metal buffer indexes. The program ABI adds
 * lookup_palette at 2, the plan constants as words at 10, the policy's
 * workspace at SIXEL_LOOKUP_WORKSPACE_BINDING (11) and an invalid-index
 * flag at 12. The dispatch range lives in push constants.
 */
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require


layout(std430, set = 0, binding = 0) writeonly buffer Result {
    uint8_t result[];
};
layout(std430, set = 0, binding = 1) readonly buffer Pixels {
    uint8_t pixels[];
};
layout(std430, set = 0, binding = 2) readonly buffer LookupPalette {
    uint8_t lookup_palette[];
};
layout(std430, set = 0, binding = 3) readonly buffer TransparentMask {
    uint8_t transparent_mask[];
};
layout(std430, set = 0, binding = 4) readonly buffer BlueNoise {
    uint8_t blue_noise[];
};
layout(std430, set = 0, binding = 5) readonly buffer Gradient {
    uint8_t gradient[];
};
/* Field order and sizes match sixel_gpu_vulkan_params_t. */
layout(std430, set = 0, binding = 6) readonly buffer Params {
    uint pixel_count;
    uint width;
    uint height;
    uint ncolors;
    uint reserved;
    uint has_transparent;
    uint transparent_size;
    uint transparent_keycolor;
    uint has_6delta;
    uint accumulation_pixel_count;
    uint accumulation_valid_mask_size;
    uint accumulation_keycolor;
    uint accumulation_stride, accumulation_origin;
    uint sixdelta_threshold;
    uint sixdelta_metric;
    uint sixdelta_compare;
    uint residual_threshold;
    uint residual_columns, residual_rows;
    uint residual_tiled;
    uint residual_cells[128];
    uint accumulation_result_mask_size;
    float positional_strength;
    float bluenoise_strength;
    float bluenoise_gradient_factor;
    int bluenoise_phase_x;
    int bluenoise_phase_y;
    uint bluenoise_channel_rgb;
    uint gradient_size;
    uint gradient_width;
    uint gradient_height;
    uint8_t sixdelta_palette[768];
} params;
layout(std430, set = 0, binding = 7) readonly buffer Accumulation {
    uint8_t accumulation[];
};
layout(std430, set = 0, binding = 8) readonly buffer AccumulationValid {
    uint8_t accumulation_valid_mask[];
};
layout(std430, set = 0, binding = 9) writeonly buffer AccumulationResult {
    uint8_t accumulation_result_mask[];
};
layout(std430, set = 0, binding = 10) readonly buffer LookupConstants {
    uint lookup_constants[];
};
#define SIXEL_LOOKUP_WORKSPACE_BINDING 11
/* One validation word per in-flight row range. */
layout(std430, set = 0, binding = 12) buffer LookupInvalid {
    uint lookup_invalid[];
};

/* first: first pixel of this dispatch; count: pixels in this dispatch;
 * pitch: invocations per dispatch row (groups.x * local_size_x); flag: the
 * range's validation word. */
layout(push_constant) uniform Range {
    uint first;
    uint count;
    uint pitch;
    uint flag;
} range;

uint sixdelta_palette_distance(uvec3 q, uint palette_index)
{
    int dr = int(q.x) - int(uint(params.sixdelta_palette[palette_index * 3u]));
    int dg = int(q.y) -
        int(uint(params.sixdelta_palette[palette_index * 3u + 1u]));
    int db = int(q.z) -
        int(uint(params.sixdelta_palette[palette_index * 3u + 2u]));
    return uint(dr * dr + dg * dg + db * db);
}

uint color_distance(uvec3 left, uvec3 right)
{
    ivec3 d = ivec3(left) - ivec3(right);
    return uint(d.x * d.x + d.y * d.y + d.z * d.z);
}

uvec3 load_accumulation(uint index)
{
    return uvec3(uint(accumulation[index * 3u + 0u]),
                 uint(accumulation[index * 3u + 1u]),
                 uint(accumulation[index * 3u + 2u]));
}

/* Build exactly the byte query consumed by lookup, including truncation
 * and clipping. */
uvec3 positional_query(uvec3 source, uint x, uint y, uint gid)
{
    return source;
}
bool prepare_query(uint gid, out uvec3 query, out uvec3 comparison)
{
    uint x;
    uint y;
    uint local_threshold, cell;
    uint retained_index;
    bool within;
    int dr;
    int dg;
    int db;
    query = uvec3(0u);
    comparison = uvec3(0u);
    if (gid >= params.pixel_count) {
        return true;
    }
    if (params.has_transparent != 0u && gid < params.transparent_size &&
        uint(transparent_mask[gid]) != 0u) {
        result[gid] = uint8_t(params.transparent_keycolor);
        if (gid < params.accumulation_result_mask_size) {
            accumulation_result_mask[gid] = uint8_t(1u);
        }
        return true;
    }
    x = gid % params.width;
    y = gid / params.width;
    retained_index = params.accumulation_origin +
        y * params.accumulation_stride + x;
    comparison = uvec3(uint(pixels[gid * 3u]), uint(pixels[gid * 3u + 1u]),
        uint(pixels[gid * 3u + 2u]));
    if (params.sixdelta_compare != 0u) {
        query = positional_query(comparison, x, y, gid);
        comparison = query;
    }
    if (params.has_6delta != 0u &&
        gid < params.accumulation_pixel_count &&
        (gid >= params.accumulation_valid_mask_size ||
         uint(accumulation_valid_mask[retained_index]) != 0u)) {
        uvec3 retained = load_accumulation(retained_index);
        dr = abs(int(comparison.x) - int(retained.x));
        dg = abs(int(comparison.y) - int(retained.y));
        db = abs(int(comparison.z) - int(retained.z));
        local_threshold = params.sixdelta_threshold;
        if (params.residual_columns != 0u) {
            if (params.residual_tiled != 0u)
                cell = (y & 63u) * 64u + (x & 63u);
            else
                cell = (((y + 1u) * params.residual_rows - 1u) /
                    params.height) * params.residual_columns +
                    ((x + 1u) * params.residual_columns - 1u) /
                    params.width;
            if ((params.residual_cells[cell / 32u] &
                    (1u << (cell % 32u))) != 0u)
                local_threshold = min(local_threshold,
                    params.residual_threshold);
        }
        within = uint(dr) <= local_threshold &&
            uint(dg) <= local_threshold && uint(db) <= local_threshold;
        if (params.sixdelta_metric == 2u)
            within = uint(2 * dr * dr + 4 * dg * dg + db * db) <=
                7u * local_threshold * local_threshold;
        if (within) {
            result[gid] = uint8_t(params.accumulation_keycolor);
            if (gid < params.accumulation_result_mask_size) {
                accumulation_result_mask[gid] =
                    uint8_t((dr | dg | db) != 0 ? 3u : 1u);
            }
            return true;
        }
    }
    if (params.sixdelta_compare == 0u) {
        query = positional_query(comparison, x, y, gid);
    }
    return false;
}

void finish_pixel(uint gid, uvec3 comparison, uint best)
{
    uint retained_index = params.accumulation_origin +
        (gid / params.width) * params.accumulation_stride +
        gid % params.width;
    if (params.has_6delta != 0u &&
        gid < params.accumulation_pixel_count &&
        (gid >= params.accumulation_valid_mask_size ||
         uint(accumulation_valid_mask[retained_index]) != 0u)) {
        if (color_distance(comparison, load_accumulation(retained_index)) <=
            sixdelta_palette_distance(comparison, best)) {
            result[gid] = uint8_t(params.accumulation_keycolor);
            if (gid < params.accumulation_result_mask_size) {
                accumulation_result_mask[gid] = uint8_t(1u);
            }
            return;
        }
    }
    result[gid] = uint8_t(best);
}
