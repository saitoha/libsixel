/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that scheduler allocation consumes an already effective sampling
 * policy instead of selecting sampling as a scheduling side effect.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/frame.h"
#include "src/planner.h"

static int
sampling_scheduler_contract_valid(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_encoding_planner_t planner;
    unsigned char pixels[6 * 12 * 3];

    status = SIXEL_FALSE;
    allocator = NULL;
    encoder = NULL;
    frame = NULL;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_init_borrowed(frame,
                                       pixels,
                                       6,
                                       12,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       -1);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_encoding_planner_init(&planner);
    planner.total_threads = 4;
    planner.heavy_ops = 1;
    status = sixel_encoding_planner_schedule(
        &planner,
        encoder,
        frame,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME);
    if (SIXEL_FAILED(status) || planner.main_threads != 4 ||
            planner.palette_threads != 0 ||
            planner.allow_palette_async != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    status = sixel_encoding_planner_schedule(
        &planner,
        encoder,
        frame,
        SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID);
    if (SIXEL_FAILED(status) || planner.main_threads != 3 ||
            planner.palette_threads != 1 ||
            planner.allow_palette_async != 1) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    planner.total_threads = 2;
    planner.heavy_ops = 1;
    status = sixel_encoding_planner_schedule(
        &planner,
        encoder,
        frame,
        SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID);
    if (SIXEL_FAILED(status) || planner.main_threads != 2 ||
            planner.palette_threads != 0 ||
            planner.allow_palette_async != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    planner.total_threads = 1;
    planner.heavy_ops = 0;
    status = sixel_encoding_planner_schedule(
        &planner,
        encoder,
        frame,
        SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID);
    if (SIXEL_FAILED(status) || planner.main_threads != 1 ||
            planner.palette_threads != 0 ||
            planner.allow_palette_async != 0) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    status = SIXEL_OK;

cleanup:
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status);
}

int
test_palette_0028_sampling_schedule(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!sampling_scheduler_contract_valid()) {
        fprintf(stderr, "sampling scheduler contract failed\n");
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
