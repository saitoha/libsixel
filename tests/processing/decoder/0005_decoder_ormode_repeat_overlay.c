/*
 * SPDX-License-Identifier: MIT
 *
 * Verify OR mode repeat spans compose each touched pixel independently.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

static unsigned char g_ormode_payload[] =
    "\033P7;5q\"1;1;3;1"
    "#0;2;0;0;0"
    "#1;2;100;0;0"
    "#2;2;0;100;0"
    "#3;2;0;0;100"
    "#1!3@$#2!2@\033\\";

int
test_decoder_0005_decoder_ormode_repeat_overlay(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *palette;
    int width;
    int height;
    int ncolors;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    pixels = NULL;
    palette = NULL;
    width = 0;
    height = 0;
    ncolors = 0;
    ok = 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decode_raw(g_ormode_payload,
                              (int)(sizeof(g_ormode_payload) - 1U),
                              &pixels,
                              &width,
                              &height,
                              &palette,
                              &ncolors,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (width != 3 || height != 1) {
        fprintf(stderr, "unexpected repeat dimensions %dx%d\n",
                width,
                height);
        goto end;
    }
    if (pixels[0] != 3 || pixels[1] != 3 || pixels[2] != 1) {
        fprintf(stderr,
                "OR mode repeat indexes are %u,%u,%u, expected 3,3,1\n",
                pixels[0],
                pixels[1],
                pixels[2]);
        goto end;
    }

    ok = ncolors >= 4;

end:
    if (allocator != NULL) {
        if (palette != NULL) {
            sixel_allocator_free(allocator, palette);
        }
        if (pixels != NULL) {
            sixel_allocator_free(allocator, pixels);
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
