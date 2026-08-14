/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that every newly supported error-diffusion policy honors the
 * 6delta skip mode.  A kept pixel must not alter the following source pixel.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/dither.h"
#include "src/filter.h"
#include "tests/processing/filter/filter_test_common.h"

int
test_filter_0026_filter_dither_6delta_diffusion_skip(int argc, char **argv)
{
    static int const methods[] = {
        SIXEL_DIFFUSE_JAJUNI,
        SIXEL_DIFFUSE_STUCKI,
        SIXEL_DIFFUSE_BURKES,
        SIXEL_DIFFUSE_SIERRA1,
        SIXEL_DIFFUSE_SIERRA2,
        SIXEL_DIFFUSE_SIERRA3
    };
    static char const *const names[] = {
        "jajuni",
        "stucki",
        "burkes",
        "sierra1",
        "sierra2",
        "sierra3"
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    unsigned char palette[6];
    unsigned char pixel[6];
    unsigned char retained[6];
    unsigned char valid_mask[2];
    size_t method_index;
    int ok;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    indexes = NULL;
    method_index = 0U;
    ok = 0;
    palette[0] = 0u;
    palette[1] = 0u;
    palette[2] = 0u;
    palette[3] = 255u;
    palette[4] = 255u;
    palette[5] = 255u;
    retained[0] = 255u;
    retained[1] = 255u;
    retained[2] = 255u;
    retained[3] = 0u;
    retained[4] = 0u;
    retained[5] = 0u;
    valid_mask[0] = 1u;
    valid_mask[1] = 0u;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "allocator setup failed: %04x\n", status);
        goto end;
    }
    for (method_index = 0U;
            method_index < sizeof(methods) / sizeof(methods[0]);
            ++method_index) {
        pixel[0] = 130u;
        pixel[1] = 130u;
        pixel[2] = 130u;
        pixel[3] = 130u;
        pixel[4] = 130u;
        pixel[5] = 130u;

        status = make_dither(allocator, 2, &dither);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr,
                    "%s dither setup failed: %04x\n",
                    names[method_index],
                    status);
            goto end;
        }
        sixel_dither_set_palette(dither, palette);
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
        sixel_dither_set_diffusion_type(dither, methods[method_index]);
        sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
        sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
        sixel_dither_set_optimize_palette(dither, 0);
        sixel_dither_set_transparent(dither, 0);
        sixel_dither_set_pipeline_accumulation_buffer_hint(
            dither,
            retained,
            sizeof(retained),
            valid_mask,
            sizeof(valid_mask),
            2,
            1,
            0,
            0,
            2,
            1,
            0,
            1,
            0u,
            SIXEL_6DELTA_ERROR_SKIP);

        indexes = sixel_dither_apply_palette(dither, pixel, 2, 1);
        if (indexes == NULL) {
            fprintf(stderr,
                    "%s palette application failed\n",
                    names[method_index]);
            goto end;
        }
        if (indexes[0] != 0) {
            fprintf(stderr,
                    "%s emitted palette index %u instead of 6delta keep\n",
                    names[method_index],
                    (unsigned int)indexes[0]);
            goto end;
        }
        if (pixel[3] != 130u || pixel[4] != 130u || pixel[5] != 130u) {
            fprintf(stderr,
                    "%s diffused kept-pixel error in skip mode: %u,%u,%u\n",
                    names[method_index],
                    (unsigned int)pixel[3],
                    (unsigned int)pixel[4],
                    (unsigned int)pixel[5]);
            goto end;
        }
        sixel_allocator_free(allocator, indexes);
        indexes = NULL;
        sixel_dither_unref(dither);
        dither = NULL;
    }
    ok = 1;

end:
    if (indexes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, indexes);
    }
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
