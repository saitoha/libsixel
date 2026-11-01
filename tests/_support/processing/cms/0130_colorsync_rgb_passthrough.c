/* SPDX-License-Identifier: MIT */
/* Excluded definitions retain the original native transform's exact result. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/forward-lut.h"
#include "tests/data/color-numeric/rgb-power.h"
#include "tests/data/color-numeric/rgb-power-exclusions.h"

int
test_cms_0130(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    unsigned char const *bytes;
    size_t length;
    float pixels[18] = {0.0f, 0.0f, 0.0f, 0.0001f, 0.0002f, 0.0003f,
        0.1f, 0.2f, 0.3f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f};
    float expected[18];
    int variant;
    int strict;
    int p;
    int ok;

    if (argc != 3) {
        return 1;
    }
    variant = atoi(argv[1]);
    strict = strcmp(argv[2], "strict") == 0;
    if (variant < 0 || variant >= 8) {
        return 1;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    bytes = color_icc;
    length = sizeof(color_icc);
    if (variant == 2 || variant == 4 || variant == 5) {
        bytes = color_rgb_power_exclusions[variant == 2 ? 0 : variant - 3]
            .bytes;
        length = color_rgb_power_exclusions[variant == 2 ? 0 : variant - 3]
            .length;
    } else if (variant == 3) {
        bytes = color_forward_lut;
        length = sizeof(color_forward_lut);
    } else if (variant >= 6) {
        bytes = color_rgb_power_profiles[33].bytes;
        length = color_rgb_power_profiles[33].length;
        /* Every triplet stays outside the curve-rewrite domain. These
         * uniform curves require no channel exchange. Preserve in-range
         * neighboring components and negative darks; mixed-buffer
         * eligibility is covered by separate value tests. */
        for (p = 0; p < 6; ++p) {
            pixels[p * 3 + 1] = variant == 6
                ? (p == 2 ? -0.25f : -0.001f) : 1.25f;
        }
    }
    source = variant == 0 ? sixel_cms_create_srgb_profile()
        : sixel_cms_open_profile_from_mem(bytes, length);
    destination = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, destination,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    /* This direct transform bypasses source preparation. It fixes retained
     * native behavior, separately from the independent pure-power oracle. */
    ok = transform != NULL && sixel_cms_transform_engine(transform) ==
        SIXEL_CMS_ENGINE_COLORSYNC && sixel_cms_do_transform(transform,
            pixels, expected, 6);
    sixel_cms_delete_transform(transform);
    if (ok) {
        ok = strict ? sixel_cms_convert_colorsync_linear(pixels, 6, 1,
            variant == 0, variant == 0 ? NULL : bytes,
            variant == 0 ? 0u : length)
            : sixel_cms_convert_profile_to_linearrgb((unsigned char *)pixels,
                6, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, source);
    }
    ok = ok && memcmp(pixels, expected, sizeof(pixels)) == 0;
    if (!ok) {
        fprintf(stderr, "native passthrough changed: variant=%d strict=%d\n",
            variant, strict);
    }
    sixel_cms_close_profile(destination);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
