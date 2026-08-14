/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that A-dither lets the retained 6delta color compete with the
 * palette entry selected from its jittered lookup sample.  The comparison
 * must use the original source color, and keeping must be recorded explicitly.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/compat_stub.h"
#include "src/dither.h"
#include "src/filter.h"
#include "tests/processing/filter/filter_test_common.h"

int
test_filter_0022_filter_dither_6delta_a_dither(int argc, char **argv)
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
    char const *strength_env;
    char *saved_strength;
    size_t result_mask_size;
    size_t saved_strength_size;
    int result_value;
    int strength_overridden;
    int ok;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;
    dither = NULL;
    indexes = NULL;
    result_mask = NULL;
    strength_env = NULL;
    saved_strength = NULL;
    result_mask_size = 0U;
    saved_strength_size = 0U;
    result_value = -1;
    strength_overridden = 0;
    ok = 0;
    palette[0] = 255u;
    palette[1] = 255u;
    palette[2] = 255u;
    palette[3] = 96u;
    palette[4] = 106u;
    palette[5] = 115u;
    pixel[0] = 128u;
    pixel[1] = 128u;
    pixel[2] = 128u;
    retained[0] = 130u;
    retained[1] = 130u;
    retained[2] = 130u;
    valid_mask[0] = 1u;

    strength_env = sixel_compat_getenv(
        "SIXEL_DITHER_A_DITHER_STRENGTH");
    if (strength_env != NULL) {
        saved_strength_size = strlen(strength_env) + 1U;
        saved_strength = (char *)malloc(saved_strength_size);
        if (saved_strength == NULL) {
            fprintf(stderr, "A-dither strength save allocation failed\n");
            goto end;
        }
        memcpy(saved_strength, strength_env, saved_strength_size);
    }
    if (sixel_compat_setenv("SIXEL_DITHER_A_DITHER_STRENGTH", "1") != 0) {
        fprintf(stderr, "A-dither strength setup failed\n");
        goto end;
    }
    strength_overridden = 1;

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
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_A_DITHER);
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
        1,
        1,
        0,
        0,
        1,
        1,
        0,
        1,
        0u,
        SIXEL_6DELTA_ERROR_DIFFUSE);
    sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);

    /*
     * At strength 1, A-dither maps (128,128,128) to (96,106,115).
     * The retained (130,130,130) is closer only to the unjittered source.
     */
    indexes = sixel_dither_apply_palette(dither, pixel, 1, 1);
    if (indexes == NULL) {
        fprintf(stderr, "A-dither palette application failed\n");
        goto end;
    }
    result_mask = sixel_dither_get_pipeline_accumulation_result_mask(
        dither,
        &result_mask_size);
    if (result_mask != NULL && result_mask_size == 1U) {
        result_value = (int)result_mask[0];
    }
    if (indexes[0] != 0 || result_value != 1) {
        fprintf(stderr,
                "A-dither did not record 6delta keep: index=%u mask=%d\n",
                (unsigned int)indexes[0],
                result_value);
        goto end;
    }
    ok = 1;

end:
    if (strength_overridden != 0 && sixel_compat_setenv(
            "SIXEL_DITHER_A_DITHER_STRENGTH",
            saved_strength != NULL ? saved_strength : "") != 0) {
        fprintf(stderr, "A-dither strength restore failed\n");
        ok = 0;
    }
    free(saved_strength);
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
