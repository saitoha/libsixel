/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_test_common.h"

int
test_gamut_0015_encoder_isolation(int argc, char **argv)
{
    sixel_encoder_t *first = NULL;
    sixel_encoder_t *second = NULL;
    double result[3];
    double retained[3];
    double source[3] = {1.1, -.02, .02};
    int ok;

    (void)argc;
    (void)argv;
    ok = SIXEL_SUCCEEDED(sixel_encoder_new(&first, NULL)) &&
        SIXEL_SUCCEEDED(sixel_encoder_new(&second, NULL)) &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(first, 'U',
            "gamma:gamut=local-minde")) &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(second, 'U',
            "display-p3:gamut=local-minde")) &&
        sixel_gamut_apply(first->gamut_mapper, source, result,
            SIXEL_COLORSPACE_LINEAR) && result[0] <= 1.000001 &&
        sixel_gamut_apply(second->gamut_mapper, source, retained,
            SIXEL_COLORSPACE_LINEAR) &&
        memcmp(retained, source, sizeof(source)) == 0;
    sixel_encoder_unref(first);
    ok = ok && sixel_gamut_apply(second->gamut_mapper, source, retained,
        SIXEL_COLORSPACE_LINEAR) &&
        memcmp(retained, source, sizeof(source)) == 0;
    sixel_encoder_unref(second);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
