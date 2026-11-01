/* SPDX-License-Identifier: MIT */
/* Literal runs retain exact clipping flags at both trusted raster edges. */
#include <stdio.h>
#include <stdlib.h>
#include <sixel.h>
#include "src/sixel_decode_pixels.h"

int
test_decoder_0100(int argc, char **argv)
{
    unsigned char input[128];
    unsigned char *pixels;
    unsigned char *mask;
    unsigned char *palette;
    unsigned int flags;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int length;
    int mode;
    int x;
    int y;
    int painted;
    int ok;

    (void)argc;
    (void)argv;
    pixels = mask = palette = NULL;
    ok = 0;
    for (mode = 0; mode < 2; ++mode) {
        length = snprintf((char *)input, sizeof(input),
            "\033P%d;%dq\"1;1;3;7#1;2;100;0;0~?~~-$?~~?\033\\",
            mode ? 7 : 0, mode ? 5 : 1);
        status = sixel_decode_raw_with_options_mask(input, length,
            SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE,
            &pixels, &mask, &width, &height, &palette, &ncolors,
            &flags, NULL);
        if (SIXEL_FAILED(status) || width != 3 || height != 7 ||
                ncolors != 2 || flags !=
                (SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER |
                 SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER)) {
            goto done;
        }
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                painted = y < 6 ? x != 1 : x != 0;
                if (mask[y * width + x] != (mode || painted ? 255 : 0) ||
                        pixels[y * width + x] !=
                        (painted ? 1 : (mode ? 0 : 255))) {
                    goto done;
                }
            }
        }
        free(pixels); free(mask); free(palette);
        pixels = mask = palette = NULL;
    }
    ok = 1;
done:
    free(pixels); free(mask); free(palette);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
