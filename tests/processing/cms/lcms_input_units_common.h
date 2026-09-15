/* SPDX-License-Identifier: MIT */
/* Shared setup for one source/target/alias unit-conversion observation. */
#ifndef TEST_LCMS_INPUT_UNITS_COMMON_H
#define TEST_LCMS_INPUT_UNITS_COMMON_H
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/lcms-input-units.h"

static int
lcms_input_units(int lab, int gamma, int alias)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    float input[521 * 4 + 8];
    float original[521 * 4 + 8];
    float output[521 * 3 + 8];
    float *result;
    double const (*bounds)[3];
    size_t channels;
    size_t samples;
    size_t pixel;
    size_t sample;
    size_t channel;
    size_t i;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_LCMS2);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_LCMS2) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    channels = lab ? 3u : 4u;
    samples = lab ? sizeof(cms_units_lab_source) /
        sizeof(cms_units_lab_source[0]) : sizeof(cms_units_cmyk_source) /
        sizeof(cms_units_cmyk_source[0]);
    for (i = 0u; i < sizeof(input) / sizeof(input[0]); ++i) {
        input[i] = -99.0f;
    }
    for (i = 0u; i < sizeof(output) / sizeof(output[0]); ++i) {
        output[i] = -99.0f;
    }
    for (pixel = 0u; pixel < 521u; ++pixel) {
        /* Change the second/third batch's sequence as well as its length.
         * Reusing the first batch cannot accidentally satisfy the oracle. */
        sample = (pixel + pixel / 256u * 17u) % samples;
        for (channel = 0u; channel < channels; ++channel) {
            input[4u + pixel * channels + channel] = lab
                ? cms_units_lab_source[sample][channel]
                : cms_units_cmyk_source[sample][channel];
        }
    }
    memcpy(original, input, sizeof(input));
    result = alias ? input + 4 : output + 4;
    source = sixel_cms_open_profile_from_mem(lab ? cms_units_lab_icc
        : cms_units_cmyk_icc, lab ? sizeof(cms_units_lab_icc)
        : sizeof(cms_units_cmyk_icc));
    target = gamma ? sixel_cms_create_srgb_profile()
                   : sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source, lab
        ? SIXEL_CMS_PIXELFORMAT_LAB_F32 : SIXEL_CMS_PIXELFORMAT_CMYK_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = sixel_cms_transform_engine(transform) == SIXEL_CMS_ENGINE_LCMS2 &&
        sixel_cms_do_transform(transform, input + 4, result, 521u);
    for (pixel = 0u; ok && pixel < 521u; ++pixel) {
        sample = (pixel + pixel / 256u * 17u) % samples;
        bounds = lab ? cms_units_lab_bounds[sample][gamma]
                     : cms_units_cmyk_bounds[sample][gamma];
        for (channel = 0u; channel < 3u; ++channel) {
            if (!isfinite(result[pixel * 3u + channel]) ||
                result[pixel * 3u + channel] < bounds[0][channel] ||
                result[pixel * 3u + channel] > bounds[1][channel]) {
                fprintf(stderr, "LCMS units pixel=%zu channel=%zu "
                    "value=%.9g interval=[%.9g,%.9g]\n", pixel, channel,
                    result[pixel * 3u + channel], bounds[0][channel],
                    bounds[1][channel]);
                ok = 0;
            }
        }
    }
    /* CMYK contraction may change only the RGB output span. Preserve the
     * remaining source tail, outer guards and all separate-input bytes. */
    for (i = 0u; i < sizeof(input) / sizeof(input[0]); ++i) {
        if ((!alias || i < 4u || i >= 4u + 521u * 3u) &&
            memcmp(input + i, original + i, sizeof(float)) != 0) {
            fprintf(stderr, "LCMS input or alias-tail modified: %zu\n", i);
            ok = 0;
        }
    }
    for (i = 0u; i < 4u; ++i) {
        if (output[i] != -99.0f || output[4u + 521u * 3u + i] != -99.0f) {
            fprintf(stderr, "LCMS output guard modified: %zu\n", i);
            ok = 0;
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "LCMSUNITS|lab=%d|gamma=%d|alias=%d|count=521|"
            "values-and-guards-verified\n", lab, gamma, alias);
    }
    return ok ? 0 : 1;
}
#endif
