/* SPDX-License-Identifier: MIT */
#include <metal_stdlib>
using namespace metal;

struct YcbcrTaps {
    int first, count;
    float weights[17];
};
struct YcbcrParams {
    uint sw, cw, width, height;
    uint cb_offset, cr_offset, chroma_step, bt1886;
    float offset, scale, matrix[4];
};

static float ycbcr_luma(device const uchar *src, uint stride,
    device const YcbcrTaps &h, device const YcbcrTaps &v)
{
    float value = 0;
    for (int j = 0; j < v.count; ++j) {
        device const uchar *line = src + (v.first + j) * stride + h.first;
        float sum = 0;
        for (int i = 0; i < h.count; ++i) {
            sum += float(line[i]) * h.weights[i];
        }
        value += sum * v.weights[j];
    }
    return value;
}

static float2 ycbcr_chroma(device const uchar *src, constant YcbcrParams &p,
    device const YcbcrTaps &h, device const YcbcrTaps &v)
{
    float2 value = 0;
    for (int j = 0; j < v.count; ++j) {
        uint k = ((v.first + j) * p.cw + h.first) * p.chroma_step;
        float2 sum = 0;
        for (int i = 0; i < h.count; ++i, k += p.chroma_step) {
            sum += float2(src[p.cb_offset + k], src[p.cr_offset + k]) *
                h.weights[i];
        }
        value += sum * v.weights[j];
    }
    return value;
}

static uchar ycbcr_pack(float x, uint bt1886)
{
    if (bt1886 != 0 && x > 0) {
        /* Compose x^2.4 with the matching nonlinear sRGB inverse power.
         * Only the linear toe of the sRGB encoder still needs a power. */
        x = x <= 0.090473846f ? 12.92f * pow(x, 2.4f) : 1.055f * x - 0.055f;
    }
    return uchar(clamp(x * 255.0f + 0.5f, 0.0f, 255.0f));
}

/* Accel: SCALE-YCBCR-RGB8 metal */
kernel void ycbcr_rgb8(device const uchar *src [[buffer(0)]],
    device uchar *dst [[buffer(1)]], device const YcbcrTaps *tap [[buffer(2)]],
    constant YcbcrParams &p [[buffer(3)]], uint i [[thread_position_in_grid]])
{
    if (i >= p.width * p.height) return;
    uint x = i % p.width, y = i / p.width;
    uint vertical = 2 * p.width + y;
    float luma = (ycbcr_luma(src, p.sw, tap[x], tap[vertical]) -
        p.offset) * p.scale;
    float2 uv = ycbcr_chroma(src, p, tap[p.width + x],
        tap[vertical + p.height]) - 128.0f;
    dst[3 * i] = ycbcr_pack(luma + uv.y * p.matrix[0], p.bt1886);
    dst[3 * i + 1] = ycbcr_pack(luma - uv.x * p.matrix[1] -
        uv.y * p.matrix[2], p.bt1886);
    dst[3 * i + 2] = ycbcr_pack(luma + uv.x * p.matrix[3], p.bt1886);
}
