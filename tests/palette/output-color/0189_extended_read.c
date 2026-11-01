/* SPDX-License-Identifier: MIT */
/* Coherent compatibility reads preserve signed and extended coordinates */
#include "../../_support/quant/palette/retention/retention_test_common.h"

int
test_palette_retention_extended_read(int argc, char **argv)
{
    float values[6] = {-.25f, 1.25f, .0149f, .5f, .25f, .5f};
    unsigned char bytes[6] = {0, 255, 4, 128, 64, 128};
    sixel_palette_entries_request_t byte_request;
    sixel_palette_float32_entries_request_t float_request;
    sixel_palette_float32_entries_view_t view;
    sixel_dither_t *dither;
    int ok;

    (void)argc;
    (void)argv;
    dither = NULL;
    ok = 0;
    memset(&byte_request, 0, sizeof(byte_request));
    memset(&float_request, 0, sizeof(float_request));
    byte_request.entries = bytes;
    byte_request.colors = 2;
    byte_request.depth = 3;
    float_request.entries = values;
    float_request.colors = 2;
    float_request.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, NULL)) ||
            SIXEL_FAILED(dither->palette->vtbl->init_entries(dither->palette,
                &byte_request)) ||
            SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
                dither->palette, &float_request))) {
        goto end;
    }
    /* The compatibility projection is clipped, but it has not been edited.
     * Reconciliation must be an exact identity, without arithmetic stores. */
    ok = memcmp(retention_bytes(dither), bytes, sizeof(bytes)) == 0 &&
        SIXEL_SUCCEEDED(sixel_palette_reconcile_bytes(dither->palette,
            SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
        SIXEL_SUCCEEDED(dither->palette->vtbl->get_entries_float32(
            dither->palette, &view)) && view.entries != NULL &&
        memcmp(view.entries, values, sizeof(values)) == 0;
end:
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
