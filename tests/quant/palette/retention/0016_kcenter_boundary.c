/* SPDX-License-Identifier: MIT */
/* Compatibility projection rounding cannot replace K-center float values. */
#include "retention_test_common.h"

int
test_palette_retention_kcenter_boundary(int argc, char **argv)
{
    float pixels[6] = {
        .5039215683937073f, .5039215683937073f, .5039215683937073f,
        .9f, .9f, .9f
    };
    sixel_dither_t *dither;
    int dark;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    ok = 0;
    if (!retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_QUANTIZE_MODEL_KCENTER) || !retention_pair(dither, pixels)) {
        goto end;
    }
    dark = retention_bytes(dither)[0] < retention_bytes(dither)[3] ? 0 : 1;
    /* The first float lies just below 128.5/255. Double scaling and float
     * scaling can choose different byte neighbors. Either byte rounding
     * is permissible across FPUs; an unchanged getter must preserve the
     * actual representative, not replace it with either byte neighbor. */
    ok = retention_pick(dither, pixels, dark) &&
        retention_pair(dither, pixels);
end:
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
