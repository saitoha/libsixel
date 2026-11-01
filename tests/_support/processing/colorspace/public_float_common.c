/* SPDX-License-Identifier: MIT */
/* Public float adapters must use the extended arithmetic contract. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/colorspace.h"
#include "src/options.h"
#include "src/threading.h"
#include "tests/data/color-numeric/opponents.h"

static int
public_float_values(int mode)
{
    /* Decimal(60) evaluations of the signed sRGB curves and the documented
     * SMPTE-C matrices. Input -.25, 1.25, .5 is exactly binary32. These
     * constants are not a roundtrip through the production converter. */
    static double const expected[][3] = {
        {-.53709873048319425, 1.1027948987207226, .73535698305244949},
        {-.050876088171556791, 1.6659398809545144, .21404114048223244},
        {-.61283121928523914, 1.1235123567230623, .73120111643737919},
        {.039710951737133123, 1.5806216092152059, .21182693111414714}
    };
    static int const spaces[] = {
        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB,
        SIXEL_COLORSPACE_DIN99D
    };
    static int const formats[] = {
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    float pixels[257 * 3];
    sixel_runtime_policy_options_t runtime;
    size_t count;
    size_t i;
    int channel;
    int src;
    int dst;
    int format;
    int column;
    int reference;
    double actual;
    double value;
    double tolerance;

    count = mode == 17 ? 257U : 1U;
    src = SIXEL_COLORSPACE_LINEAR;
    dst = SIXEL_COLORSPACE_GAMMA;
    format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    column = 0;
    reference = 0;
    tolerance = 2e-7;
    if (mode == 2) {
        src = SIXEL_COLORSPACE_GAMMA;
        dst = SIXEL_COLORSPACE_LINEAR;
        format = SIXEL_PIXELFORMAT_RGBFLOAT32;
        reference = 1;
    } else if (mode >= 3 && mode <= 8) {
        count = sizeof(color_opponents) / sizeof(color_opponents[0]);
        column = 3 * (1 + (mode - 3) / 2);
        /* Stored input and final float rounding, plus inverse sensitivity
         * over these fixed [-1.28, 1.59] fixtures, budget 5e-6 coordinate
         * units. Clipping loses at least .01 on the targeted witnesses. */
        tolerance = 5e-6;
        if (mode % 2) {
            dst = spaces[(mode - 3) / 2];
        } else {
            src = spaces[(mode - 3) / 2];
            format = formats[(mode - 3) / 2];
            dst = SIXEL_COLORSPACE_LINEAR;
        }
    } else if (mode == 9) {
        dst = SIXEL_COLORSPACE_SMPTEC;
        reference = 2;
    } else if (mode == 10) {
        src = SIXEL_COLORSPACE_SMPTEC;
        dst = SIXEL_COLORSPACE_LINEAR;
        format = SIXEL_PIXELFORMAT_RGBFLOAT32;
        reference = 3;
    }
    for (i = 0; i < count; ++i) {
        for (channel = 0; channel < 3; ++channel) {
            if (mode >= 3 && mode <= 8) {
                pixels[i * 3 + channel] = (float)color_opponents[i][
                    (mode % 2 ? 0 : column) + channel];
            } else {
                pixels[i * 3 + channel] = channel == 0 ? -.25f :
                    channel == 1 ? 1.25f : .5f;
            }
        }
    }
    if (mode == 17) {
        sixel_runtime_policy_load(&runtime);
        runtime.colorspace_parallel_min_pixels = 1;
        runtime.colorspace_parallel_min_pixels_override = 1;
        sixel_runtime_policy_store(&runtime);
        sixel_set_encoder_threads(3);
    }
    if (SIXEL_FAILED(sixel_helper_convert_colorspace(
            (unsigned char *)pixels, count * 3 * sizeof(float), format,
            src, dst))) {
        return EXIT_FAILURE;
    }
    for (i = 0; i < count; ++i) {
        for (channel = 0; channel < 3; ++channel) {
            value = mode >= 3 && mode <= 8 ? color_opponents[i][
                (mode % 2 ? column : 0) + channel] :
                expected[reference][channel];
            actual = pixels[i * 3 + channel];
            if (!isfinite(actual) || fabs(actual - value) > tolerance) {
                fprintf(stderr, "%d/%lu/%d: %.9g != %.9g\n", mode,
                    (unsigned long)i, channel, actual, value);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

static int
public_float_failure(int mode)
{
    float pixels[] = {-.25f, 1.25f, .5f, .2f, .3f, .4f};
    float original[6];
    size_t size;
    int src;
    int dst;
    SIXELSTATUS status;

    size = sizeof(pixels);
    src = SIXEL_COLORSPACE_LINEAR;
    dst = SIXEL_COLORSPACE_GAMMA;
    switch (mode) {
    case 11: pixels[3] = NAN; break;
    case 12: pixels[3] = INFINITY; break;
    case 13:
        pixels[3] = FLT_MAX;
        src = SIXEL_COLORSPACE_GAMMA;
        dst = SIXEL_COLORSPACE_LINEAR;
        break;
    case 14:
        pixels[3] = pixels[4] = pixels[5] = -1;
        dst = SIXEL_COLORSPACE_DIN99D;
        break;
    case 15: dst = -1; break;
    case 16: size -= 1U; break;
    default: return EXIT_FAILURE;
    }
    memcpy(original, pixels, sizeof(pixels));
    status = sixel_helper_convert_colorspace((unsigned char *)pixels, size,
        SIXEL_PIXELFORMAT_RGBFLOAT32, src, dst);
    return status == SIXEL_BAD_INPUT &&
        memcmp(original, pixels, sizeof(pixels)) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_color_public_float(int argc, char **argv)
{
    unsigned char bytes[] = {0, 128, 255, 64, 192, 1};
    unsigned char const expected[] = {0, 55, 255, 13, 134, 0};
    float identity[] = {-.25f, 1.25f, NAN};
    float original[3];
    int mode;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    mode = atoi(argv[1]);
    if ((mode >= 1 && mode <= 10) || mode == 17) {
        return public_float_values(mode);
    }
    if (mode >= 11 && mode <= 16) {
        return public_float_failure(mode);
    }
    if (mode == 18) {
        /* Explicit byte storage keeps its original LUT rounding. */
        return SIXEL_SUCCEEDED(sixel_helper_convert_colorspace(bytes,
            sizeof(bytes), SIXEL_PIXELFORMAT_RGB888,
            SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR)) &&
            memcmp(bytes, expected, sizeof(bytes)) == 0
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (mode == 19) {
        /* A no-op has always preserved bits, including nonfinite samples. */
        memcpy(original, identity, sizeof(identity));
        return SIXEL_SUCCEEDED(sixel_helper_convert_colorspace(
            (unsigned char *)identity, sizeof(identity),
            SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_GAMMA)) &&
            memcmp(identity, original, sizeof(identity)) == 0
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    return EXIT_FAILURE;
}
