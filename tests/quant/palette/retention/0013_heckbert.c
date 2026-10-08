/* SPDX-License-Identifier: MIT */
/* Ordinary Heckbert retains source float precision through dither setup */
#include "retention_test_common.h"

int
test_palette_retention_heckbert(int argc, char **argv)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    sixel_dither_t *dither;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    /* Ordinary histogram generation and dither publication both retain the
     * input fractions. Neither may rebuild this view from byte entries. */
    ok = retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_QUANTIZE_MODEL_MEDIANCUT) && retention_pair(dither, pixels);
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
