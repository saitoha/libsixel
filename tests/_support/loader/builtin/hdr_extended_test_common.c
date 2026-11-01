/* SPDX-License-Identifier: MIT */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "loader_builtin_memory_test_common.h"
#include "hdr_extended_test_common.h"

int
hdr_extended_test(int specimen)
{
    /* Inverse RGB-to-XYZ matrix independently derived from sRGB xy
     * primaries and D65, with each primary scaled to the white point.
     * Columns are the expected RGB values for unit X, Y and Z. */
    static double const rgb[3][3] = {
        {3.240969941904521, -0.9692436362808798, 0.05563007969699361},
        {-1.537383177570093, 1.875967501507721, -0.2039769588889766},
        {-0.4986107602930033, 0.04155505740717562, 1.056971514242879}
    };
    unsigned char file[256];
    unsigned char native[4][4];
    char header[160];
    edge_writer_t w;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float actual[24];
    double expected;
    double scale;
    unsigned int width;
    unsigned int pixel;
    unsigned int channel;
    unsigned int j;
    int length;
    int result;

    width = specimen == 2 ? 8u : 4u;
    memset(native, 0, sizeof(native));
    for (j = 0u; j < 3u; ++j) {
        native[j][j] = specimen == 7 ? 255u : 128u;
        native[j][3] = specimen == 7 ? 255u : 129u;
    }
    /* A mixed XYZ sample exposes negative red; the high-exponent variant
     * also reaches the negative float32 representability boundary. */
    native[3][0] = specimen == 7 ? 0u : 24u;
    native[3][1] = specimen == 7 ? 255u : 48u;
    native[3][2] = specimen == 7 ? 255u : 16u;
    native[3][3] = specimen == 7 ? 255u : 129u;
    length = snprintf(header, sizeof(header),
        "#?RADIANCE\nFORMAT=32-bit_rle_xyze\n%s\n-Y 1 +X %u\n",
        specimen == 3 ? "EXPOSURE=0.5\n" : specimen == 4 ?
        "COLORCORR=2 4 0.5\n" : specimen == 6 ? "EXPOSURE=1e-300\n" :
        "", width);
    if (length < 0 || (size_t)length >= sizeof(header)) {
        return 1;
    }
    w = (edge_writer_t){file, sizeof(file), 0u, 0};
    edge_put_bytes(&w, (unsigned char *)header, (size_t)length);
    if (specimen == 2) {
        edge_put_u8(&w, 2u);
        edge_put_u8(&w, 2u);
        edge_put_u16be(&w, width);
        for (channel = 0u; channel < 4u; ++channel) {
            edge_put_u8(&w, width);
            for (pixel = 0u; pixel < width; ++pixel) {
                edge_put_u8(&w, native[pixel % 4u][channel]);
            }
        }
    } else {
        edge_put_bytes(&w, &native[0][0], sizeof(native));
    }
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer("HDR signed XYZ", file, w.length, 1,
                              &probe, &status);
    if (w.failed || result != 0 || SIXEL_FAILED(status) ||
        probe.callback_count != 1 || probe.width[0] != (int)width ||
        probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_LINEAR ||
        probe.mask_size[0] != 0u ||
        probe.rgb_size[0] != width * 3u * sizeof(float)) {
        fprintf(stderr, "HDR extended case=%d status=%x format=%x\n",
                specimen, status, probe.pixelformat[0]);
        return 1;
    }
    memcpy(actual, probe.rgb[0], probe.rgb_size[0]);
    for (pixel = 0u; pixel < width; ++pixel) {
        for (channel = 0u; channel < 3u; ++channel) {
            expected = 0.0;
            for (j = 0u; j < 3u; ++j) {
                expected += native[pixel % 4u][j] * rgb[j][channel];
            }
            expected *= specimen == 7 ? ldexp(1.0, 119) : 1.0 / 128.0;
            expected = fmax(-FLT_MAX, fmin(FLT_MAX, expected));
            /* Native float publication precedes exposure, so include
             * that representation rounding in the independent oracle. */
            expected = (float)expected;
            scale = specimen == 3 ? 2.0 : specimen == 6 ? 1e300 : 1.0;
            if (specimen == 4) {
                scale = channel == 0u ? 0.5 : channel == 1u ? 0.25 : 2.0;
            }
            expected *= scale;
            if (specimen == 5) {
                expected = fmax(0.0, expected);
                expected /= 1.0 + expected;
            }
            expected = fmax(-FLT_MAX, fmin(FLT_MAX, expected));
            /* Relative plus absolute budget covers float publication,
             * without a CMS approximation or perceptual threshold. */
            if (!isfinite(actual[pixel * 3u + channel]) ||
                fabs(actual[pixel * 3u + channel] - expected) >
                    2e-7 * fmax(1.0, fabs(expected))) {
                fprintf(stderr, "HDR case=%d pixel=%u channel=%u "
                        "%.9g != %.9g\n", specimen, pixel, channel,
                        actual[pixel * 3u + channel], expected);
                return 1;
            }
        }
    }
    return 0;
}
