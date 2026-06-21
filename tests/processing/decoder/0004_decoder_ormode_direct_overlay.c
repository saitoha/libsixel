/*
 * SPDX-License-Identifier: MIT
 *
 * Verify OR mode direct-color decoding converts the composed palette index,
 * not the last bit-plane selector, into RGBA output.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/decoder-parallel.h"

static unsigned char g_ormode_payload[] =
    "\033P7;5q\"1;1;1;1"
    "#0;2;0;0;0"
    "#1;2;100;0;0"
    "#2;2;0;100;0"
    "#3;2;0;0;100"
    "#1@$#2@\033\\";

int
test_decoder_0004_decoder_ormode_direct_overlay(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    int width;
    int height;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    pixels = NULL;
    width = 0;
    height = 0;
    ok = 0;

    status = sixel_decoder_parallel_override_threads("2");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decode_direct(g_ormode_payload,
                                 (int)(sizeof(g_ormode_payload) - 1U),
                                 &pixels,
                                 &width,
                                 &height,
                                 allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (width != 1 || height != 1) {
        fprintf(stderr, "unexpected direct dimensions %dx%d\n",
                width,
                height);
        goto end;
    }
    if (pixels[0] != 0 ||
            pixels[1] != 0 ||
            pixels[2] != 255 ||
            pixels[3] != 255) {
        fprintf(stderr,
                "OR mode direct RGBA is %u,%u,%u,%u, expected blue\n",
                pixels[0],
                pixels[1],
                pixels[2],
                pixels[3]);
        goto end;
    }

    ok = 1;

end:
    (void)sixel_decoder_parallel_override_threads("1");
    if (allocator != NULL) {
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
