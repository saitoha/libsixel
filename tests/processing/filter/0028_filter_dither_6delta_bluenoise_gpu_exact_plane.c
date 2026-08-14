/*
 * SPDX-License-Identifier: MIT
 *
 * Exercise the high-level dither path that attaches an exact retained plane
 * to a forced GPU blue-noise request.
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
test_filter_0028_filter_dither_6delta_bluenoise_gpu_exact_plane(
    int argc,
    char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    unsigned char palette[6];
    unsigned char pixel[3];
    unsigned char retained[3];
    unsigned char valid_mask[1];
    unsigned char const *result_mask;
    size_t result_mask_size;
    int ok;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    indexes = NULL;
    result_mask = NULL;
    result_mask_size = 0U;
    ok = 0;
    palette[0] = 0u;
    palette[1] = 0u;
    palette[2] = 0u;
    palette[3] = 255u;
    palette[4] = 255u;
    palette[5] = 255u;
    pixel[0] = 140u;
    pixel[1] = 140u;
    pixel[2] = 140u;
    retained[0] = 255u;
    retained[1] = 255u;
    retained[2] = 255u;
    valid_mask[0] = 1u;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = make_dither(allocator, 2, &dither);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(
        dither,
        SIXEL_DIFFUSE_BLUENOISE_DITHER);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    sixel_dither_set_transparent(dither, 7);
    dither->gpu_policy = SIXEL_GPU_POLICY_FORCE;
    dither->bluenoise_strength_override = 1;
    dither->bluenoise_strength = 1.0f;
    dither->bluenoise_phase_override = 1;
    dither->bluenoise_phase_x = 48;
    dither->bluenoise_phase_y = 52;
    sixel_dither_set_pipeline_accumulation_buffer_hint(
        dither,
        retained,
        sizeof(retained),
        valid_mask,
        sizeof(valid_mask),
        1,
        1,
        0,
        0,
        1,
        1,
        7,
        1,
        0U,
        SIXEL_6DELTA_ERROR_DIFFUSE);
    sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);

    indexes = sixel_dither_apply_palette(dither, pixel, 1, 1);
    if (indexes == NULL) {
        fprintf(stderr, "forced GPU blue-noise application failed\n");
        goto end;
    }
    result_mask = sixel_dither_get_pipeline_accumulation_result_mask(
        dither,
        &result_mask_size);
    if (indexes[0] != 7u || result_mask == NULL ||
            result_mask_size != 1U || result_mask[0] != 1u) {
        fprintf(stderr, "exact-plane GPU 6delta result mismatch\n");
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
