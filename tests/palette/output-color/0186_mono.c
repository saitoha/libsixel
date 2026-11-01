/* SPDX-License-Identifier: MIT */
/* Float lookup uses retained fractions near monochrome endpoints */
#include "../../_support/quant/palette/retention/retention_test_common.h"

int
test_palette_retention_mono(int argc, char **argv)
{
    float pixels[6] = {.0002f, .0002f, .0002f, .9999f, .9999f, .9999f};
    float query[3] = {.500025f, .500025f, .500025f};
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
    dark = bytes[0] == 0 ? 0 : 1;
    /* Byte endpoints would put the midpoint at .5 and select white.
     * The actual midpoint is .50005, safely above this query. */
    ok = bytes[dark * 3] == 0 && bytes[(1 - dark) * 3] == 255 &&
        retention_pick(dither, query, dark) && retention_pair(dither, pixels);
end:
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
