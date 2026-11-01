/* SPDX-License-Identifier: MIT */
/* Explicit identical byte assignment replaces float precision */
#include "../../_support/quant/palette/retention/retention_test_common.h"

int
test_palette_retention_setter_same(int argc, char **argv)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    float query[3] = {.2648f, .2648f, .2648f};
    float assigned[6] = {4.f/255, 4.f/255, 4.f/255,
        131.f/255, 131.f/255, 131.f/255};
    unsigned char bytes[6];
    sixel_dither_t *dither;
    int dark;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    ok = 0;
    if (!retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_QUANTIZE_MODEL_KMEANS)) {
        goto end;
    }
    memcpy(bytes, retention_bytes(dither), sizeof(bytes));
    dark = bytes[0] < bytes[3] ? 0 : 1;
    /* Float midpoint .2649 selects dark. Explicit byte assignment makes
     * the midpoint 135/510 = .26470588..., which selects light. */
    if (!retention_pick(dither, query, dark)) {
        goto end;
    }
    sixel_dither_set_palette(dither, bytes);
    ok = retention_pick(dither, query, 1 - dark) &&
        retention_pair(dither, assigned);
end:
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
