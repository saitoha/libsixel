/* SPDX-License-Identifier: MIT */
/* GPU lookup must never select an appended omission key as a real color. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sixel.h>
#include "src/dither.h"

int
test_gpupal_0002_reserved_key(int argc, char **argv)
{
    unsigned char palette[9] = {60, 60, 60, 200, 200, 200, 0, 0, 0};
    unsigned char pixel[3] = {0, 0, 0};
    unsigned char retained[3] = {255, 255, 255};
    unsigned char valid[1] = {1};
    int const lookups[] = {SIXEL_LUT_POLICY_NONE, SIXEL_LUT_POLICY_EYTZINGER};
    int const methods[] = {SIXEL_DIFFUSE_NONE, SIXEL_DIFFUSE_BLUENOISE_DITHER};
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    unsigned char const *mask;
    size_t mask_size;
    SIXELSTATUS status;
    int lookup, method, ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    dither = NULL;
    indexes = NULL;
    ok = 0;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (lookup = 0; lookup < 2; ++lookup) {
        for (method = 0; method < 2; ++method) {
            status = sixel_dither_new(&dither, 3, allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            sixel_dither_set_palette(dither, palette);
            sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
            sixel_dither_set_diffusion_type(dither, methods[method]);
            sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
            sixel_dither_set_lut_policy(dither, lookups[lookup]);
            sixel_dither_set_optimize_palette(dither, 0);
            sixel_dither_set_transparent(dither, 2);
            dither->keycolor_reserved = 1;
            dither->gpu_policy = SIXEL_GPU_POLICY_FORCE;
            dither->bluenoise_strength_override = 1;
            dither->bluenoise_strength = 0.5f;
            dither->bluenoise_phase_override = 1;
            dither->bluenoise_phase_x = 48;
            dither->bluenoise_phase_y = 52;
            sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
                retained, sizeof(retained), valid, sizeof(valid),
                1, 1, 0, 0, 1, 1, 2, 1, 8U, SIXEL_6DELTA_ERROR_DIFFUSE);
            sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);

            /* Black source: the reserved black slot would win if admitted.
             * Of the two paintable grays, 60 is nearest with or without the
             * bounded positional perturbation. White history cannot win a
             * legitimate keep. Assert both index and omission metadata. */
            indexes = sixel_dither_apply_palette(dither, pixel, 1, 1);
            mask = sixel_dither_get_pipeline_accumulation_result_mask(
                dither, &mask_size);
            if (indexes == NULL || indexes[0] != 0 || mask == NULL ||
                    mask_size != 1U || mask[0] != 0) {
                fprintf(stderr, "reserved key entered GPU lookup %d/%d\n",
                    lookup, method);
                goto end;
            }
            sixel_allocator_free(allocator, indexes);
            indexes = NULL;
            sixel_dither_unref(dither);
            dither = NULL;
        }
    }
    ok = 1;

end:
    if (allocator != NULL) {
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
