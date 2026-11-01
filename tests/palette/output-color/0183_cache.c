/* SPDX-License-Identifier: MIT */
/* Unchanged palette application preserves prepared output values */
#include "../../_support/quant/palette/retention/retention_test_common.h"

int
test_palette_retention_cache(int argc, char **argv)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    sixel_dither_t *dither;
    unsigned char const *codes;
    unsigned char saved[6];
    unsigned int count;
    int dark;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    ok = 0;
    if (!retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_QUANTIZE_MODEL_KMEANS) ||
            SIXEL_FAILED(sixel_palette_prepare_rgb101(dither->palette,
                SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY)) ||
            SIXEL_FAILED(sixel_palette_get_rgb101(dither->palette,
                &codes, &count)) || count != 2) {
        goto end;
    }
    memcpy(saved, codes, sizeof(saved));
    dark = codes[0] < codes[3] ? 0 : 1;
    ok = retention_pick(dither, pixels, dark) &&
        retention_pick(dither, pixels + 3, 1 - dark) &&
        SIXEL_SUCCEEDED(sixel_palette_get_rgb101(dither->palette,
            &codes, &count)) && count == 2 &&
        memcmp(saved, codes, sizeof(saved)) == 0 &&
        retention_pair(dither, pixels);
end:
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
