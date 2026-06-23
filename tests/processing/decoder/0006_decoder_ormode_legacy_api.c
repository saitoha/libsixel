/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the deprecated sixel_decode() API follows the same OR-mode rules as
 * sixel_decode_raw().  Some embedders still use this compatibility entry point,
 * so palette index 0 must remain a real color instead of an implicit hole.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#if defined(__GNUC__)
#define LSQA_BEGIN_DEPRECATED_DECLARATIONS \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define LSQA_END_DEPRECATED_DECLARATIONS \
    _Pragma("GCC diagnostic pop")
#else
#define LSQA_BEGIN_DEPRECATED_DECLARATIONS
#define LSQA_END_DEPRECATED_DECLARATIONS
#endif

static unsigned char g_ormode_legacy_payload[] =
    "\033P7;5q\"1;1;2;1"
    "#0;2;25;50;75"
    "#1;2;100;0;0"
    "#2;2;0;100;0"
    "#3;2;0;0;100"
    "#1@$#2@\033\\";

int
test_decoder_0006_decoder_ormode_legacy_api(int argc, char **argv)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    unsigned char *palette;
    int width;
    int height;
    int ncolors;
    int ok;

    (void)argc;
    (void)argv;

    pixels = NULL;
    palette = NULL;
    width = 0;
    height = 0;
    ncolors = 0;
    ok = 0;

    LSQA_BEGIN_DEPRECATED_DECLARATIONS;
    status = sixel_decode(g_ormode_legacy_payload,
                          (int)(sizeof(g_ormode_legacy_payload) - 1U),
                          &pixels,
                          &width,
                          &height,
                          &palette,
                          &ncolors,
                          NULL);
    LSQA_END_DEPRECATED_DECLARATIONS;
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (width != 2 || height != 1) {
        fprintf(stderr, "unexpected legacy dimensions %dx%d\n",
                width,
                height);
        goto end;
    }
    if (ncolors < 4) {
        fprintf(stderr, "OR mode did not expose legacy palette index 3\n");
        goto end;
    }
    if (pixels[0] != 3 || pixels[1] != 0) {
        fprintf(stderr,
                "OR mode legacy indexes are %u,%u, expected 3,0\n",
                pixels[0],
                pixels[1]);
        goto end;
    }
    if (palette[0] != 64 || palette[1] != 128 || palette[2] != 191) {
        fprintf(stderr,
                "OR mode legacy palette #0 is %u,%u,%u, expected #0\n",
                palette[0],
                palette[1],
                palette[2]);
        goto end;
    }

    ok = 1;

end:
    free(palette);
    free(pixels);
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
