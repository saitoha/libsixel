/* SPDX-License-Identifier: MIT */
/* Invalid float reconciliation cannot partially rewrite a palette */
#include "retention_test_common.h"

int
test_palette_retention_invalid(int argc, char **argv)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    sixel_palette_float32_entries_view_t view;
    sixel_dither_t *dither;
    unsigned char *bytes;
    float *values;
    float saved;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    ok = 0;
    if (!retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_QUANTIZE_MODEL_KMEANS) ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL) {
        goto end;
    }
    bytes = retention_bytes(dither);
    values = (float *)view.entries;
    saved = values[0];
    bytes[0] = 80;
    values[5] = NAN;
    /* A changed early byte must not hide a later nonfinite coordinate.
     * Rejection leaves even the first finite coordinate untouched. */
    ok = sixel_palette_reconcile_bytes(dither->palette,
        SIXEL_PIXELFORMAT_RGBFLOAT32) == SIXEL_BAD_INPUT &&
        values[0] == saved && isnan(values[5]);
end:
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
