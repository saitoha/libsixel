/* SPDX-License-Identifier: MIT */
/* Shared setup for one source/target/alias unit-conversion observation. */
#ifndef TEST_COLORSYNC_LAB_UNITS_COMMON_H
#define TEST_COLORSYNC_LAB_UNITS_COMMON_H
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/colorsync-lab.h"

static int
colorsync_lab_units(int gamma, int alias)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    float input[521 * 3 + 8];
    float original[521 * 3 + 8];
    float output[521 * 3 + 8];
    float *result;
    double const (*bounds)[3];
    size_t samples;
    size_t pixel;
    size_t sample;
    size_t channel;
    size_t i;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    samples = sizeof(cs_lab_source) / sizeof(cs_lab_source[0]);
    for (i = 0u; i < sizeof(input) / sizeof(input[0]); ++i) {
        input[i] = -99.0f;
    }
    for (i = 0u; i < sizeof(output) / sizeof(output[0]); ++i) {
        output[i] = -99.0f;
    }
    for (pixel = 0u; pixel < 521u; ++pixel) {
        /* Change the second/third batch's sequence as well as its length.
         * Reusing the first batch cannot accidentally satisfy the oracle. */
        sample = (pixel + pixel / 256u * 37u) % samples;
        for (channel = 0u; channel < 3u; ++channel) {
            input[4u + pixel * 3u + channel] =
                cs_lab_source[sample][channel];
        }
    }
    memcpy(original, input, sizeof(input));
    result = alias ? input + 4 : output + 4;
    source = sixel_cms_open_profile_from_mem(cs_lab_icc, sizeof(cs_lab_icc));
    /* Fixed independent targets isolate input units from profile factories. */
    target = sixel_cms_open_profile_from_mem(gamma ? cs_lab_gamma_icc
        : cs_lab_linear_icc, gamma ? sizeof(cs_lab_gamma_icc)
        : sizeof(cs_lab_linear_icc));
    transform = sixel_cms_create_transform(source,
        SIXEL_CMS_PIXELFORMAT_LAB_F32, target, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = sixel_cms_transform_engine(transform) == SIXEL_CMS_ENGINE_COLORSYNC &&
        sixel_cms_do_transform(transform, input + 4, result, 521u);
    for (pixel = 0u; ok && pixel < 521u; ++pixel) {
        sample = (pixel + pixel / 256u * 37u) % samples;
        bounds = cs_lab_bounds[sample][gamma];
        for (channel = 0u; channel < 3u; ++channel) {
            if (!isfinite(result[pixel * 3u + channel]) ||
                result[pixel * 3u + channel] < bounds[0][channel] ||
                result[pixel * 3u + channel] > bounds[1][channel]) {
                fprintf(stderr, "ColorSync Lab units pixel=%zu channel=%zu "
                    "value=%.9g interval=[%.9g,%.9g]\n", pixel, channel,
                    result[pixel * 3u + channel], bounds[0][channel],
                    bounds[1][channel]);
                ok = 0;
            }
        }
    }
    /* Lab and RGB share the same width. Preserve the outer guards and
     * every byte of a separate input, including the final partial batch. */
    for (i = 0u; i < sizeof(input) / sizeof(input[0]); ++i) {
        if ((!alias || i < 4u || i >= 4u + 521u * 3u) &&
            memcmp(input + i, original + i, sizeof(float)) != 0) {
            fprintf(stderr, "ColorSync Lab input or alias-tail modified: "
                "%zu\n", i);
            ok = 0;
        }
    }
    for (i = 0u; i < 4u; ++i) {
        if (output[i] != -99.0f || output[4u + 521u * 3u + i] != -99.0f) {
            fprintf(stderr, "ColorSync Lab output guard modified: %zu\n", i);
            ok = 0;
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "CSLABUNITS|gamma=%d|alias=%d|count=521|"
            "values-and-guards-verified\n", gamma, alias);
    }
    return ok ? 0 : 1;
}
#endif
