/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that the transitional sample-stream artifact distinguishes borrowed
 * full-frame views from owned adaptive samples.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/sample-stream.h"

static int
sample_stream_ownership_is_valid(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *borrowed_frame;
    sixel_frame_t *owned_frame;
    sixel_frame_t *owned_identity;
    sixel_sample_stream_t stream;
    unsigned char borrowed_pixels[12];
    unsigned char owned_pixels[18];

    status = SIXEL_FALSE;
    allocator = NULL;
    borrowed_frame = NULL;
    owned_frame = NULL;
    owned_identity = NULL;
    sixel_sample_stream_init(&stream);

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_new(&borrowed_frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_init_borrowed(borrowed_frame,
                                       borrowed_pixels,
                                       2,
                                       2,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       -1);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_sample_stream_bind_borrowed(
        &stream,
        borrowed_frame,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    if (SIXEL_FAILED(status) ||
            stream.frame != borrowed_frame ||
            stream.storage != SIXEL_SAMPLE_STREAM_BORROWED_FRAME ||
            stream.policy != SIXEL_PALETTE_SAMPLING_FULL_FRAME ||
            stream.source !=
                SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME ||
            stream.point_count != 4u ||
            stream.width != 2 || stream.height != 2 ||
            stream.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            stream.colorspace != SIXEL_COLORSPACE_GAMMA) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    status = sixel_sample_stream_bind_borrowed(
        &stream,
        borrowed_frame,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    if (status != SIXEL_LOGIC_ERROR) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_sample_stream_dispose(&stream);
    if (sixel_frame_get_width(borrowed_frame) != 2 ||
            stream.storage != SIXEL_SAMPLE_STREAM_EMPTY) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }

    status = sixel_frame_new(&owned_frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_init_borrowed(owned_frame,
                                       owned_pixels,
                                       3,
                                       2,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       -1);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    owned_identity = owned_frame;
    status = sixel_sample_stream_take_owned(
        &stream,
        &owned_frame,
        SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID,
        SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME);
    if (SIXEL_FAILED(status) || owned_frame != NULL ||
            stream.frame != owned_identity ||
            stream.storage != SIXEL_SAMPLE_STREAM_OWNED_FRAME ||
            stream.policy != SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID ||
            stream.source != SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME ||
            stream.point_count != 6u) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_frame_set_colorspace(stream.frame, SIXEL_COLORSPACE_OKLAB);
    status = sixel_sample_stream_refresh(&stream);
    if (SIXEL_FAILED(status) ||
            stream.colorspace != SIXEL_COLORSPACE_OKLAB ||
            stream.point_count != 6u ||
            stream.storage != SIXEL_SAMPLE_STREAM_OWNED_FRAME) {
        status = SIXEL_LOGIC_ERROR;
        goto cleanup;
    }
    sixel_sample_stream_dispose(&stream);
    status = SIXEL_OK;

cleanup:
    sixel_sample_stream_dispose(&stream);
    sixel_frame_unref(owned_frame);
    sixel_frame_unref(borrowed_frame);
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status);
}

int
test_palette_0025_sample_stream(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!sample_stream_ownership_is_valid()) {
        fprintf(stderr, "sample stream ownership contract failed\n");
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
