/*
 * SPDX-License-Identifier: MIT
 *
 * Verify K-center canonicalizes packed alpha layouts before shared binning.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/dither.h"

int
test_palette_0044_kcenter_packed_alpha_layouts(int argc, char **argv)
{
    static unsigned char const pixels[4][16] = {
        {
            251u, 2u, 3u, 0u,
            200u, 10u, 20u, 255u,
            15u, 180u, 25u, 128u,
            30u, 40u, 210u, 1u
        },
        {
            0u, 251u, 2u, 3u,
            255u, 200u, 10u, 20u,
            128u, 15u, 180u, 25u,
            1u, 30u, 40u, 210u
        },
        {
            3u, 2u, 251u, 0u,
            20u, 10u, 200u, 255u,
            25u, 180u, 15u, 128u,
            210u, 40u, 30u, 1u
        },
        {
            0u, 3u, 2u, 251u,
            255u, 20u, 10u, 200u,
            128u, 25u, 180u, 15u,
            1u, 210u, 40u, 30u
        }
    };
    static int const formats[4] = {
        SIXEL_PIXELFORMAT_RGBA8888,
        SIXEL_PIXELFORMAT_ARGB8888,
        SIXEL_PIXELFORMAT_BGRA8888,
        SIXEL_PIXELFORMAT_ABGR8888
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_palette_entries_view_t view;
    unsigned char expected[12];
    unsigned int format_index;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    memset(&view, 0, sizeof(view));
    memset(expected, 0, sizeof(expected));
    format_index = 0u;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    for (format_index = 0u; format_index < 4u; ++format_index) {
        status = sixel_dither_new(&dither, 4, allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        dither->quantize_model = SIXEL_QUANTIZE_MODEL_KCENTER;
        dither->final_merge_mode = SIXEL_FINAL_MERGE_NONE;
        dither->keycolor = 0;
        status = sixel_dither_initialize(
            dither,
            (unsigned char *)pixels[format_index],
            4,
            1,
            formats[format_index],
            SIXEL_LARGE_AUTO,
            SIXEL_REP_AUTO,
            SIXEL_QUALITY_AUTO);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = dither->palette->vtbl->get_entries(dither->palette, &view);
        if (SIXEL_FAILED(status) || view.entry_count != 3u ||
                view.depth != 3 || view.entries == NULL) {
            status = SIXEL_LOGIC_ERROR;
            goto cleanup;
        }
        if (format_index == 0u) {
            memcpy(expected, view.entries, 9u);
        } else if (memcmp(expected, view.entries, 9u) != 0) {
            status = SIXEL_LOGIC_ERROR;
            goto cleanup;
        }
        sixel_dither_unref(dither);
        dither = NULL;
    }
    status = SIXEL_OK;

cleanup:
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "K-center packed-alpha layout regression failed\n");
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
