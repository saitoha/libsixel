/* SPDX-License-Identifier: MIT */
/* Compatibility Heckbert retains its typed byte-histogram export */
#include "retention_test_common.h"

int
test_palette_retention_heckbert(int argc, char **argv)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    float expected[6] = {4.f/255, 4.f/255, 4.f/255,
        131.f/255, 131.f/255, 131.f/255};
    sixel_dither_t *dither;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    /* This boundary does not undo the compatibility histogram's earlier
     * byte quantization. Keeping the view must not claim that precision. */
    ok = retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_QUANTIZE_MODEL_MEDIANCUT) && retention_pair(dither, expected);
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
