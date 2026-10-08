/* SPDX-License-Identifier: MIT */
/* Byte construction remains eligible without a float palette */
#include "retention_test_common.h"

int
test_palette_retention_byte(int argc, char **argv)
{
    int const models[] = {SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        SIXEL_QUANTIZE_MODEL_KMEANS, SIXEL_QUANTIZE_MODEL_KCENTER,
        SIXEL_QUANTIZE_MODEL_KMEDOIDS};
    unsigned char pixels[6] = {4, 4, 4, 131, 131, 131};
    sixel_palette_float32_entries_view_t view;
    sixel_dither_t *dither;
    sixel_index_t *indices;
    int model;
    int ok;

    (void)argc;
    (void)argv;
    for (model = 0; model < 4; ++model) {
        dither = NULL;
        indices = NULL;
        ok = retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGB888,
            models[model]);
        if (ok) {
            indices = sixel_dither_apply_palette(dither, pixels, 2, 1);
            memset(&view, 0, sizeof(view));
            ok = indices != NULL && indices[0] != indices[1] &&
                SIXEL_SUCCEEDED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) && view.entries == NULL &&
                dither->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
                !dither->prefer_float32 && !dither->clamp_promoted_rgb888;
            sixel_allocator_free(dither->allocator, indices);
        }
        sixel_dither_unref(dither);
        if (!ok) {
            return 1;
        }
    }
    return 0;
}
