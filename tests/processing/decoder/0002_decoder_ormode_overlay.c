/*
 * SPDX-License-Identifier: MIT
 *
 * Regression coverage for OR mode sixel decoding.  P2=5 changes color
 * selectors into bit-plane selectors, so repainting the same pixel with
 * #1 and #2 must produce palette index 3 instead of the last selector.
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

static int
decoder_ormode_raw_index_matches(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *palette;
    int width;
    int height;
    int ncolors;
    int ok;

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
    if (width != 1 || height != 1) {
        fprintf(stderr, "unexpected raw dimensions %dx%d\n", width, height);
        goto end;
    }
    if (ncolors < 4) {
        fprintf(stderr, "OR mode did not expose palette index 3\n");
        goto end;
    }
    if (pixels[0] != 3) {
        fprintf(stderr, "OR mode raw index is %u, expected 3\n", pixels[0]);
        goto end;
    }
    if (palette[3 * 3 + 0] != 0 ||
            palette[3 * 3 + 1] != 0 ||
            palette[3 * 3 + 2] != 255) {
        fprintf(stderr, "palette index 3 is not blue\n");
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
    return ok;
}

static int
decoder_ormode_direct_color_matches(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    int width;
    int height;
    int ok;

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
    return ok;
}

int
test_decoder_0002_decoder_ormode_overlay(int argc, char **argv)
{
    int ok;

    (void)argc;
    (void)argv;

    ok = decoder_ormode_raw_index_matches();
    if (!ok) {
        return EXIT_FAILURE;
    }
    ok = decoder_ormode_direct_color_matches();
    if (!ok) {
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
