/*
 * SPDX-License-Identifier: MIT
 *
 * Verify shared hard binning preserves typed K-center representatives.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/dither.h"

int
test_palette_0043_kcenter_float_grid_values(int argc, char **argv)
{
    static float const expected[] = {
        .64f, -.30f, .35f,
        .08f, .30f, -.20f,
        .75f, .00f, .00f,
        .48f, -.05f, -.45f
    };
    static float pixels[] = {
        0.49f, 0.00f, 0.00f, 0.50f, 0.00f, 0.00f,
        0.08f, 0.30f, -0.20f, 0.16f, -0.25f, 0.20f,
        0.24f, 0.10f, 0.35f, 0.32f, -0.40f, -0.10f,
        0.40f, 0.45f, 0.05f, 0.48f, -0.05f, -0.45f,
        0.56f, 0.25f, 0.25f, 0.64f, -0.30f, 0.35f,
        0.72f, 0.05f, -0.30f, 0.80f, -0.15f, -0.15f,
        0.88f, 0.35f, -0.05f, 0.96f, -0.35f, 0.10f,
        0.51f, 0.00f, 0.00f, 0.75f, 0.00f, 0.00f
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    unsigned int index;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    memset(&view, 0, sizeof(view));

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_dither_new(&dither, 4, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    dither->quantize_model = SIXEL_QUANTIZE_MODEL_KCENTER;
    dither->final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    status = sixel_dither_initialize(
        dither,
        (unsigned char *)pixels,
        16,
        1,
        SIXEL_PIXELFORMAT_OKLABFLOAT32,
        SIXEL_LARGE_AUTO,
        SIXEL_REP_AUTO,
        SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = dither->palette->vtbl->get_entries_float32(dither->palette, &view);
    if (SIXEL_FAILED(status) || view.entry_count != 4u ||
            view.depth != 3 * (int)sizeof(float) || view.entries == NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    /* The fixed hard-grid fixture selects these four observed source
     * triplets. Verify coordinates rather than double-vs-float rounding at
     * a compatibility-byte midpoint (notably a=-.20). The tolerance covers
     * scale/unscale storage error and is far below one packed-byte step. */
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        if (!isfinite(view.entries[index]) ||
                fabs((double)view.entries[index] - expected[index]) > 1e-6) {
            status = SIXEL_LOGIC_ERROR;
            goto cleanup;
        }
    }
    status = SIXEL_OK;

cleanup:
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "K-center float hard-grid regression failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
