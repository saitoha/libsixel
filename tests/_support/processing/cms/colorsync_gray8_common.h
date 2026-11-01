/* SPDX-License-Identifier: MIT */
/* Gray byte normalization, independent color values, and buffer integrity. */
#ifndef TEST_COLORSYNC_GRAY8_COMMON_H
#define TEST_COLORSYNC_GRAY8_COMMON_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/colorsync-gray8.h"

static int
colorsync_gray8(int gamma, int alias)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    unsigned char *input;
    unsigned char *output;
    unsigned char *destination;
    unsigned char original[521 * 16 + 128];
    unsigned char const *profile;
    double const (*bounds)[3];
    size_t const counts[6] = { 1u, 2u, 255u, 256u, 257u, 521u };
    size_t profile_size;
    size_t kind;
    size_t test;
    size_t count;
    size_t pixel;
    size_t sample;
    size_t channel;
    size_t i;
    float value;
    int ok;

    /* Allocated arenas have no declared scalar type. The input begins at
     * a float-aligned offset and reserves the complete expanded output. */
    input = malloc(sizeof(original));
    output = malloc(sizeof(original));
    if (input == NULL || output == NULL) {
        free(input);
        free(output);
        return 1;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        free(input);
        free(output);
        return 77;
    }
    target = sixel_cms_open_profile_from_mem(gamma ? cg_gamma_icc
        : cg_linear_icc, gamma ? sizeof(cg_gamma_icc) : sizeof(cg_linear_icc));
    ok = target != NULL;
    for (kind = 0u; ok && kind < 2u; ++kind) {
        profile = kind ? cg_identity_icc : cg_gamma2_icc;
        profile_size = kind ? sizeof(cg_identity_icc) : sizeof(cg_gamma2_icc);
        source = sixel_cms_open_profile_from_mem(profile, profile_size);
        transform = sixel_cms_create_transform(source,
            SIXEL_CMS_PIXELFORMAT_GRAY_8, target, SIXEL_CMS_PIXELFORMAT_RGB_F32,
            SIXEL_CMS_TRANSFORM_DEFAULT);
        ok = sixel_cms_transform_engine(transform) ==
             SIXEL_CMS_ENGINE_COLORSYNC;
        for (test = 0u; ok && test < 6u; ++test) {
            count = counts[test];
            memset(input, 0xa5, sizeof(original));
            memset(output, 0xa5, sizeof(original));
            for (pixel = 0u; pixel < count; ++pixel) {
                /* Shift each batch so repeating the first cannot pass. */
                sample = (pixel + pixel / 256u * 37u) % (kind ? 256u : 2u);
                input[32u + pixel] = kind ? (unsigned char)sample
                                        : cg_gamma2_source[sample];
            }
            memcpy(original, input, sizeof(original));
            destination = alias ? input : output;
            ok = sixel_cms_do_transform(transform, input + 32u,
                                        destination + 32u, count);
            for (pixel = 0u; ok && pixel < count; ++pixel) {
                sample = (pixel + pixel / 256u * 37u) % (kind ? 256u : 2u);
                bounds = kind ? cg_identity_bounds[sample][gamma]
                              : cg_gamma2_bounds[sample][gamma];
                for (channel = 0u; channel < 3u; ++channel) {
                    memcpy(&value, destination + 32u +
                        (pixel * 3u + channel) * sizeof(float), sizeof(value));
                    if (!isfinite(value) || value < bounds[0][channel] ||
                        value > bounds[1][channel]) {
                        fprintf(stderr, "Gray8 kind=%zu count=%zu pixel=%zu "
                            "channel=%zu value=%.9g interval=[%.9g,%.9g]\n",
                            kind, count, pixel, channel, value,
                            bounds[0][channel], bounds[1][channel]);
                        ok = 0;
                    }
                }
            }
            /* Compare the whole separate source, including padding. Alias
             * writes may replace the input only within the output span. */
            if (!alias && memcmp(input, original, sizeof(original)) != 0) {
                fprintf(stderr, "Gray8 separate input modified\n");
                ok = 0;
            }
            for (i = 0u; i < sizeof(original); ++i) {
                if ((i < 32u || i >= 32u + count * 3u * sizeof(float)) &&
                    destination[i] != 0xa5) {
                    fprintf(stderr, "Gray8 output guard modified: %zu\n", i);
                    ok = 0;
                }
            }
        }
        sixel_cms_delete_transform(transform);
        sixel_cms_close_profile(source);
    }
    sixel_cms_close_profile(target);
    sixel_cms_set_engine(previous);
    free(input);
    free(output);
    if (ok) {
        fprintf(stderr, "CSGRAY8|gamma=%d|alias=%d|profiles=2|counts=6|"
            "values-and-guards-verified\n", gamma, alias);
    }
    return ok ? 0 : 1;
}
#endif
