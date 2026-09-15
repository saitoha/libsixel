/* SPDX-License-Identifier: MIT */
/* One builtin source/target/layout condition with independent values. */
#ifndef TEST_BUILTIN_DEVICE_COMMON_H
#define TEST_BUILTIN_DEVICE_COMMON_H
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/cmyk.h"
#include "tests/data/color-numeric/gray-samples.h"
#include "tests/data/color-numeric/builtin-device.h"

static int
builtin_device_values(int gray, int gamma, int alias)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    float input[521 * 4 + 8];
    float original[521 * 4 + 8];
    float output[521 * 3 + 8];
    float untouched[521 * 3 + 8];
    float *result;
    unsigned char *bytes;
    double const *expected;
    double scale;
    double tolerance;
    size_t samples;
    size_t pixel;
    size_t sample;
    size_t channel;
    size_t i;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    samples = gray ? 256u : sizeof(bd_cmyk_source) /
        sizeof(bd_cmyk_source[0]);
    for (i = 0u; i < sizeof(input) / sizeof(input[0]); ++i) {
        input[i] = -99.0f;
    }
    for (i = 0u; i < sizeof(output) / sizeof(output[0]); ++i) {
        output[i] = -99.0f;
    }
    bytes = (unsigned char *)(input + 4);
    for (pixel = 0u; pixel < 521u; ++pixel) {
        /* Vary the sequence throughout the span to reveal overwritten
         * inputs. This does not prescribe batching inside builtin CMS. */
        sample = (pixel + pixel / 256u * 37u) % samples;
        if (gray) {
            bytes[pixel] = (unsigned char)sample;
        } else {
            memcpy(input + 4u + pixel * 4u, bd_cmyk_source[sample],
                4u * sizeof(float));
        }
    }
    memcpy(original, input, sizeof(input));
    memcpy(untouched, output, sizeof(output));
    source = sixel_cms_open_profile_from_mem(gray ? color_gray_icc
        : color_cmyk_icc, gray ? sizeof(color_gray_icc)
        : sizeof(color_cmyk_icc));
    target = gamma ? sixel_cms_create_srgb_profile()
                   : sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source, gray
        ? SIXEL_CMS_PIXELFORMAT_GRAY_8 : SIXEL_CMS_PIXELFORMAT_CMYK_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = sixel_cms_get_engine() == SIXEL_CMS_ENGINE_BUILTIN &&
        sixel_cms_transform_engine(transform) == SIXEL_CMS_ENGINE_BUILTIN &&
        sixel_cms_do_transform(transform, input + 4, output + 4, 521u);
    /* Every condition first checks separate-source preservation. An alias
     * condition also compares its output bits with this separate result. */
    if (memcmp(input, original, sizeof(input)) != 0) {
        fprintf(stderr, "Builtin separate source modified\n");
        ok = 0;
    }
    result = alias ? input + 4 : output + 4;
    if (ok && alias) {
        ok = sixel_cms_do_transform(transform, input + 4, result, 521u);
        if (memcmp(result, output + 4, 521u * 3u * sizeof(float)) != 0) {
            fprintf(stderr, "Builtin alias differs from separate output\n");
            ok = 0;
        }
    }
    for (pixel = 0u; ok && pixel < 521u; ++pixel) {
        sample = (pixel + pixel / 256u * 37u) % samples;
        expected = gray ? bd_gray_expected[sample][gamma]
                        : bd_cmyk_expected[sample][gamma];
        scale = fmax(fabs(expected[0]),
            fmax(fabs(expected[1]), fabs(expected[2])));
        /* Existing builtin float budget: an all-zero expected triplet
         * requires exact zero. Do not add a native CMM absolute floor. */
        tolerance = scale * 64.0 / 16777216.0;
        for (channel = 0u; channel < 3u; ++channel) {
            if (!isfinite(result[pixel * 3u + channel]) ||
                fabs(result[pixel * 3u + channel] - expected[channel]) >
                    tolerance) {
                fprintf(stderr, "Builtin device pixel=%zu channel=%zu "
                    "value=%.9g expected=%.17g tolerance=%.9g\n",
                    pixel, channel, result[pixel * 3u + channel],
                    expected[channel], tolerance);
                ok = 0;
            }
        }
    }
    /* Float arrays provide alignment and enough room for byte-to-float
     * expansion. Only the RGB output span may change for an exact alias;
     * this also preserves the unused CMYK tail after contraction. */
    for (i = 0u; i < sizeof(input) / sizeof(input[0]); ++i) {
        if ((!alias || i < 4u || i >= 4u + 521u * 3u) &&
            memcmp(input + i, original + i, sizeof(float)) != 0) {
            fprintf(stderr, "Builtin input or alias-tail modified: %zu\n",
                i);
            ok = 0;
        }
    }
    for (i = 0u; i < 4u; ++i) {
        if (memcmp(output + i, untouched + i, sizeof(float)) != 0 ||
            memcmp(output + 4u + 521u * 3u + i,
                untouched + 4u + 521u * 3u + i, sizeof(float)) != 0) {
            fprintf(stderr, "Builtin output guard modified: %zu\n", i);
            ok = 0;
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "BUILTINDEVICE|gray=%d|gamma=%d|alias=%d|count=521|"
            "values-and-guards-verified\n", gray, gamma, alias);
    }
    return ok ? 0 : 1;
}
#endif
