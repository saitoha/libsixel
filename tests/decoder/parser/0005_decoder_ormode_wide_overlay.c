/*
 * SPDX-License-Identifier: MIT
 *
 * Verify OR mode wide-index decoding keeps the same bit-plane overlay
 * semantics as the byte-index decoder.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

/*
 * sixel_decode_wide() is exported from fromsixel.c but is not declared in the
 * public header.  Keep this prototype local to cover the depth-2 decoder path.
 */
SIXELSTATUS sixel_decode_wide(unsigned char *p,
                              int len,
                              unsigned short **pixels,
                              int *pwidth,
                              int *pheight,
                              unsigned char **palette,
                              int *ncolors,
                              sixel_allocator_t *allocator);

static unsigned char g_ormode_wide_payload[] =
    "\033P7;5q\"1;1;1;1"
    "#0;2;0;0;0"
    "#1;2;100;0;0"
    "#2;2;0;100;0"
    "#3;2;0;0;100"
    "#1@$#2@\033\\";

int
test_decoder_0003_decoder_ormode_wide_overlay(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned short *pixels;
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

    status = sixel_decode_wide(g_ormode_wide_payload,
                               (int)(sizeof(g_ormode_wide_payload) - 1U),
                               &pixels,
                               &width,
                               &height,
                               &palette,
                               &ncolors,
                               allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (width != 1 || height != 1) {
        fprintf(stderr, "unexpected wide dimensions %dx%d\n", width, height);
        goto end;
    }
    if (ncolors < 4) {
        fprintf(stderr, "OR mode did not expose wide palette index 3\n");
        goto end;
    }
    if (pixels[0] != 3) {
        fprintf(stderr, "OR mode wide index is %u, expected 3\n", pixels[0]);
        goto end;
    }

    ok = 1;

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
