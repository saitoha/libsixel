/* SPDX-License-Identifier: MIT */
/* Analytic source and destination preserve the extended sRGB TRC. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/cms.h"
#include "tests/data/color-numeric/trc.h"

int
test_cms_0016(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *gamma;
    sixel_cms_profile_t *linear;
    sixel_cms_transform_t *decode;
    sixel_cms_transform_t *encode;
    float source[3];
    float decoded[3];
    float encoded[3];
    size_t i;
    size_t channel;
    int ok;

    (void)argc;
    (void)argv;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    gamma = sixel_cms_create_srgb_profile();
    linear = sixel_cms_create_linear_srgb_profile();
    decode = sixel_cms_create_transform(gamma, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        linear, SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    encode = sixel_cms_create_transform(linear, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        gamma, SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = decode != NULL && encode != NULL;
    for (i = 0; ok && i < sizeof(color_trc) / sizeof(color_trc[0]); ++i) {
        for (channel = 0; channel < 3; ++channel) {
            source[channel] = (float)color_trc[i][0];
        }
        ok = sixel_cms_do_transform(decode, source, decoded, 1u) &&
            sixel_cms_do_transform(encode, source, encoded, 1u);
        for (channel = 0; ok && channel < 3; ++channel) {
            if (!isfinite(decoded[channel]) || !isfinite(encoded[channel]) ||
                fabs(decoded[channel] - color_trc[i][1]) > 0.0000005 ||
                fabs(encoded[channel] - color_trc[i][2]) > 0.0000005) {
                fprintf(stderr, "sample=%lu: decode %.9g, encode %.9g\n",
                    (unsigned long)i, decoded[channel], encoded[channel]);
                ok = 0;
            }
        }
    }
    sixel_cms_delete_transform(decode);
    sixel_cms_delete_transform(encode);
    sixel_cms_close_profile(gamma);
    sixel_cms_close_profile(linear);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
