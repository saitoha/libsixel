/* SPDX-License-Identifier: MIT */
/* Typed exports retain their coordinate scale without native output */
#include "../../_support/quant/palette/retention/retention_test_common.h"

int
test_palette_retention_typed(int argc, char **argv)
{
    int const formats[] = {SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32};
    int const models[] = {SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_QUANTIZE_MODEL_KCENTER, SIXEL_QUANTIZE_MODEL_KMEDOIDS};
    float pixels[6];
    sixel_dither_t *dither;
    int space;
    int model;
    int ok;
    int dark;

    (void)argc;
    (void)argv;
    for (space = 0; space < 4; ++space) {
        for (model = 0; model < 3; ++model) {
            pixels[0] = .2149f;
            pixels[1] = space ? -.127f : .127f;
            pixels[2] = .063f;
            pixels[3] = .7149f;
            pixels[4] = .193f;
            pixels[5] = space ? -.084f : .084f;
            dither = NULL;
            /* With two distinct samples and two slots the representatives
             * are the inputs, independent of solver ordering or distance. */
            ok = retention_init(&dither, pixels, formats[space], models[model])
                && retention_pair(dither, pixels);
            if (ok) {
                dark = retention_bytes(dither)[0] <
                    retention_bytes(dither)[3] ? 0 : 1;
                ok = retention_pick(dither, pixels, dark) &&
                    retention_pick(dither, pixels + 3, 1 - dark) &&
                    retention_pair(dither, pixels);
            }
            sixel_dither_unref(dither);
            if (!ok) {
                return 1;
            }
        }
    }
    return 0;
}
