/* SPDX-License-Identifier: MIT */
/* Mutable byte edits affect lookup before serialization */
#include "retention_test_common.h"

int
test_palette_retention_getter_write(int argc, char **argv)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    float query[3] = {.2655f, .2655f, .2655f};
    float assigned[6] = {64.f/255, 64.f/255, 64.f/255,
        131.f/255, 131.f/255, 131.f/255};
    sixel_dither_t *dither;
    unsigned char *bytes;
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
    bytes = retention_bytes(dither);
    dark = bytes[0] < bytes[3] ? 0 : 1;
    /* Initial midpoint .2649 selects light; editing dark to 64/255 moves
     * the midpoint above .38, selecting dark for the identical query. */
    if (!retention_pick(dither, query, 1 - dark)) {
        goto end;
    }
    bytes[dark * 3] = bytes[dark * 3 + 1] = bytes[dark * 3 + 2] = 64;
    ok = retention_pick(dither, query, dark) &&
        retention_pair(dither, assigned);
end:
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
