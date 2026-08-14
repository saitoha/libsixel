/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that Stucki diffusion lets the retained 6delta color compete with its
 * palette lookup.  Threshold zero must still keep a retained color that is at
 * least as accurate as the palette entry.  The error against that retained
 * color must continue through the diffusion kernel.
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
test_filter_0017_filter_dither_6delta_stucki(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    unsigned char palette[6];
    unsigned char pixel[6];
    unsigned char retained[6];
    unsigned char valid_mask[2];
    int ok;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    indexes = NULL;
    ok = 0;
    palette[0] = 0u;
    palette[1] = 0u;
    palette[2] = 0u;
    palette[3] = 255u;
    palette[4] = 255u;
    palette[5] = 255u;
    pixel[0] = 130u;
    pixel[1] = 130u;
    pixel[2] = 130u;
    pixel[3] = 130u;
    pixel[4] = 130u;
    pixel[5] = 130u;
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
    status = make_dither(allocator, 2, &dither);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "dither setup failed: %04x\n", status);
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_STUCKI);
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
        SIXEL_6DELTA_ERROR_DIFFUSE);

    indexes = sixel_dither_apply_palette(dither, pixel, 2, 1);
    if (indexes == NULL) {
        fprintf(stderr, "stucki palette application failed\n");
        goto end;
    }
    if (indexes[0] != 0) {
        fprintf(stderr,
                "stucki emitted palette index %u instead of 6delta keep\n",
                (unsigned int)indexes[0]);
        goto end;
    }
    if (pixel[3] >= 130u) {
        fprintf(stderr,
                "stucki lost kept-pixel error before the next pixel: %u\n",
                (unsigned int)pixel[3]);
        goto end;
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
