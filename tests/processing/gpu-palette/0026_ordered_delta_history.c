/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/ordered.md
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/dither.h"
#include "ordered_state_common.h"

typedef struct ordered_history {
    sixel_dither_t *dither;
    unsigned char canvas[ORDERED_STATE_PIXELS * 3];
    unsigned char valid[ORDERED_STATE_PIXELS];
    unsigned char mask[ORDERED_STATE_PIXELS];
    sixel_index_t indexes[ORDERED_STATE_PIXELS];
} ordered_history_t;

static int
ordered_history_apply(ordered_history_t *history,
                      ordered_state_fixture_t *fixture,
                      unsigned int threshold)
{
    sixel_index_t *indexes;
    unsigned char const *mask;
    size_t size;
    int pixel;
    int colors;

    colors = fixture->request.ncolors;
    sixel_dither_set_palette(history->dither, fixture->palette);
    sixel_dither_set_pipeline_accumulation_buffer_hint(history->dither,
        history->canvas, sizeof(history->canvas), history->valid,
        sizeof(history->valid), ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT,
        0, 0, ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT, colors, 1,
        threshold, SIXEL_6DELTA_ERROR_DIFFUSE);
    indexes = sixel_dither_apply_palette(history->dither, fixture->pixels,
        ORDERED_STATE_WIDTH, ORDERED_STATE_HEIGHT);
    if (indexes == NULL) {
        return 0;
    }
    mask = sixel_dither_get_pipeline_accumulation_result_mask(
        history->dither, &size);
    if (mask == NULL || size != sizeof(history->mask)) {
        sixel_allocator_free(history->dither->allocator, indexes);
        return 0;
    }
    memcpy(history->mask, mask, size);
    memcpy(history->indexes, indexes, sizeof(history->indexes));
    sixel_allocator_free(history->dither->allocator, indexes);
    /* Replay emitted indexes independently for each backend. A skip must
     * retain the previously displayed RGB, even after a palette change. */
    for (pixel = 0; pixel < ORDERED_STATE_PIXELS; ++pixel) {
        if (history->indexes[pixel] < colors) {
            memcpy(history->canvas + pixel * 3,
                fixture->palette + history->indexes[pixel] * 3, 3);
            history->valid[pixel] = 1;
        } else if (history->indexes[pixel] != colors
                || !history->valid[pixel]) {
            return 0;
        }
    }
    return 1;
}

int
test_gpupal_0026_ordered_delta_history(int argc, char **argv)
{
    ordered_state_fixture_t fixture;
    ordered_history_t histories[2];
    int backend;
    int phase;
    int pixel;
    int kept;
    int painted;
    int ok;
    unsigned int threshold;

    (void)argc;
    (void)argv;
    memset(histories, 0, sizeof(histories));
    ok = 0;
    for (backend = 0; backend < 2; ++backend) {
        if (SIXEL_FAILED(sixel_dither_new(&histories[backend].dither,
                                         34, NULL))) {
            goto end;
        }
        histories[backend].dither->gpu_policy = backend
            ? SIXEL_GPU_POLICY_FORCE : SIXEL_GPU_POLICY_OFF;
        histories[backend].dither->keycolor_reserved = 1;
        sixel_dither_set_pixelformat(histories[backend].dither,
                                     SIXEL_PIXELFORMAT_RGB888);
        sixel_dither_set_transparent(histories[backend].dither, 33);
        sixel_dither_set_lut_policy(histories[backend].dither, backend
            ? SIXEL_LUT_POLICY_ORDERED : SIXEL_LUT_POLICY_NONE);
        sixel_dither_set_diffusion_type(histories[backend].dither,
                                        SIXEL_DIFFUSE_BLUENOISE_DITHER);
        sixel_dither_set_diffusion_scan(histories[backend].dither,
                                        SIXEL_SCAN_RASTER);
        sixel_dither_set_optimize_palette(histories[backend].dither, 0);
        sixel_dither_set_pipeline_accumulation_result_enabled(
            histories[backend].dither, 1);
    }
    for (phase = 0; phase < 9; ++phase) {
        ordered_state_init(&fixture, 33, phase / 3);
        threshold = phase % 3 == 0 ? 0U : phase % 3 == 1 ? 8U : 48U;
        /* Invalidating history must force a fresh paint. Subsequent
         * frames again combine guaranteed keeps with changing pixels. */
        if (phase == 5) {
            memset(histories[0].valid, 0, sizeof(histories[0].valid));
            memset(histories[1].valid, 0, sizeof(histories[1].valid));
        }
        for (pixel = 0; phase > 0 && pixel < ORDERED_STATE_PIXELS;
                pixel += 2) {
            memcpy(fixture.pixels + pixel * 3,
                   histories[0].canvas + pixel * 3, 3);
        }
        for (backend = 0; backend < 2; ++backend) {
            if (!ordered_history_apply(&histories[backend], &fixture,
                                        threshold)) {
                goto end;
            }
        }
        if (memcmp(histories[0].indexes, histories[1].indexes,
                   sizeof(histories[0].indexes)) != 0
                || memcmp(histories[0].mask, histories[1].mask,
                          sizeof(histories[0].mask)) != 0
                || memcmp(histories[0].canvas, histories[1].canvas,
                          sizeof(histories[0].canvas)) != 0) {
            fprintf(stderr, "ordered delta history phase=%d\n", phase);
            goto end;
        }
        kept = painted = 0;
        for (pixel = 0; pixel < ORDERED_STATE_PIXELS; ++pixel) {
            kept += histories[1].indexes[pixel] == 33;
            painted += histories[1].indexes[pixel] != 33;
        }
        if ((phase == 0 || phase == 5) ? kept != 0
                : kept == 0) {
            goto end;
        }
        if (painted == 0 && (phase == 0 || phase == 3 || phase == 5
                || phase == 6)) {
            goto end;
        }
    }
    ok = 1;
end:
    for (backend = 0; backend < 2; ++backend) {
        if (histories[backend].dither != NULL) {
            sixel_dither_unref(histories[backend].dither);
        }
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
