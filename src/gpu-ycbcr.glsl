/* SPDX-License-Identifier: MIT */
/* Accel: SCALE-YCBCR-RGB8 vulkan */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
/* Standalone compute program.
 * vulkan-stages: ycbcr_rgb8
 */
layout(local_size_x = 256) in;

struct YcbcrTaps {
    int first, count;
    float weights[17];
};
struct YcbcrParams {
    uint sw, cw, width, height;
    uint cb_offset, cr_offset, chroma_step, bt1886;
    float offset, scale, matrix[4];
};

layout(std430, binding = 0) readonly buffer Source { uint8_t src[]; };
layout(std430, binding = 1) writeonly buffer Output { uint8_t dst[]; };
layout(std430, binding = 2) readonly buffer Filters { YcbcrTaps tap[]; };
layout(std430, binding = 3) readonly buffer Params { YcbcrParams p; };

float ycbcr_luma(uint stride, uint hx, uint vy)
{
    float value = 0;
    for (int j = 0; j < tap[vy].count; ++j) {
        uint line = uint(tap[vy].first + j) * stride + uint(tap[hx].first);
        float sum = 0;
        for (int i = 0; i < tap[hx].count; ++i) {
            sum += float(src[line + i]) * tap[hx].weights[i];
        }
        value += sum * tap[vy].weights[j];
    }
    return value;
}

vec2 ycbcr_chroma(uint hx, uint vy)
{
    vec2 value = vec2(0.0);
    for (int j = 0; j < tap[vy].count; ++j) {
        uint k = (uint(tap[vy].first + j) * p.cw +
                  uint(tap[hx].first)) * p.chroma_step;
        vec2 sum = vec2(0.0);
        for (int i = 0; i < tap[hx].count; ++i, k += p.chroma_step) {
            sum += vec2(float(src[p.cb_offset + k]),
                        float(src[p.cr_offset + k])) *
                tap[hx].weights[i];
        }
        value += sum * tap[vy].weights[j];
    }
    return value;
}

uint8_t ycbcr_pack(float x, uint bt1886)
{
    if (bt1886 != 0 && x > 0) {
        /* Compose x^2.4 with the matching nonlinear sRGB inverse power.
         * Only the linear toe of the sRGB encoder still needs a power. */
        x = x <= 0.090473846f ? 12.92f * pow(x, 2.4f) : 1.055f * x - 0.055f;
    }
    return uint8_t(clamp(x * 255.0f + 0.5f, 0.0f, 255.0f));
}

void ycbcr_rgb8()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= p.width * p.height) return;
    uint x = i % p.width, y = i / p.width;
    uint vertical = 2 * p.width + y;
    float luma = (ycbcr_luma(p.sw, x, vertical) - p.offset) * p.scale;
    vec2 uv = ycbcr_chroma(p.width + x, vertical + p.height) - 128.0f;
    dst[3 * i] = ycbcr_pack(luma + uv.y * p.matrix[0], p.bt1886);
    dst[3 * i + 1] = ycbcr_pack(luma - uv.x * p.matrix[1] -
        uv.y * p.matrix[2], p.bt1886);
    dst[3 * i + 2] = ycbcr_pack(luma + uv.x * p.matrix[3], p.bt1886);
}
