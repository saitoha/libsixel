/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that sixel_decode_pixels() only clips to raster attributes when the
 * caller explicitly trusts them, while still reporting painted overflow.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

static unsigned char const g_trust_raster_payload[] =
    "\033Pq\"1;1;2;5#1;2;100;0;0!2~\033\\";

int
test_decoder_0009_decoder_pixels_trust_raster_clip(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_decode_options_t options;
    sixel_decode_result_t normal;
    sixel_decode_result_t trusted;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    memset(&normal, 0, sizeof(normal));
    memset(&trusted, 0, sizeof(trusted));

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decode_pixels(g_trust_raster_payload,
                                 sizeof(g_trust_raster_payload) - 1U,
                                 NULL,
                                 &normal,
                                 allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "normal decode failed: %04x\n", status);
        goto end;
    }
    if (normal.width != 2 || normal.height != 6) {
        fprintf(stderr, "normal dimensions are %dx%d\n",
                normal.width,
                normal.height);
        goto end;
    }
    if ((normal.flags &
            SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER) == 0U) {
        fprintf(stderr, "normal decode did not report raster overflow\n");
        goto end;
    }
    if ((normal.flags & SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER) != 0U) {
        fprintf(stderr, "normal decode unexpectedly clipped to raster\n");
        goto end;
    }

    options.flags = SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE;
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_BGRX8888;
    status = sixel_decode_pixels(g_trust_raster_payload,
                                 sizeof(g_trust_raster_payload) - 1U,
                                 &options,
                                 &trusted,
                                 allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "trusted decode failed: %04x\n", status);
        goto end;
    }
    if (trusted.width != 2 || trusted.height != 5 || trusted.stride != 8) {
        fprintf(stderr, "trusted dimensions are %dx%d stride %d\n",
                trusted.width,
                trusted.height,
                trusted.stride);
        goto end;
    }
    if ((trusted.flags &
            SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER) == 0U) {
        fprintf(stderr, "trusted decode did not report raster overflow\n");
        goto end;
    }
    if ((trusted.flags & SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER) == 0U) {
        fprintf(stderr, "trusted decode did not report raster clipping\n");
        goto end;
    }
    if ((trusted.flags & SIXEL_DECODE_PIXELS_RESULT_ALPHA_OPAQUE) == 0U) {
        fprintf(stderr, "trusted decode did not report opaque alpha\n");
        goto end;
    }
    if (trusted.pixels == NULL ||
            trusted.pixels[4U * 8U + 0U] != 0x00U ||
            trusted.pixels[4U * 8U + 1U] != 0x00U ||
            trusted.pixels[4U * 8U + 2U] != 0xffU ||
            trusted.pixels[4U * 8U + 3U] != 0xffU) {
        fprintf(stderr, "trusted decode last row is not clipped red BGRX\n");
        goto end;
    }

    ok = 1;

end:
    if (allocator != NULL) {
        if (normal.pixels != NULL) {
            sixel_allocator_free(allocator, normal.pixels);
        }
        if (trusted.pixels != NULL) {
            sixel_allocator_free(allocator, trusted.pixels);
        }
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
