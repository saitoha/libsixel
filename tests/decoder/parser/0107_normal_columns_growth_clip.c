/* SPDX-License-Identifier: MIT */
/* Band copying retains sparse paint across growth, final crop and clipping. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/sixel_decode_pixels.h"

int
test_decoder_0097(int argc, char **argv)
{
    unsigned char input[] =
        "\033P0;1q\"1;1;3;7#1;2;100;0;0!5~-$?" "?!3~\033\\";
    unsigned char *raw;
    unsigned char *mask;
    unsigned char *palette;
    unsigned int flags;
    unsigned int options;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int trust;
    int x;
    int y;
    int painted;
    int ok;

    (void)argc;
    (void)argv;
    raw = mask = palette = NULL;
    ok = 0;
    for (trust = 0; trust < 2; ++trust) {
        options = trust ? SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE : 0;
        status = sixel_decode_raw_with_options_mask(input, sizeof(input) - 1,
            options, &raw, &mask, &width, &height, &palette, &ncolors,
            &flags, NULL);
        if (SIXEL_FAILED(status) || width != (trust ? 3 : 5) ||
                height != (trust ? 7 : 12) || ncolors != 2 ||
                !(flags & SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER) ||
                !!(flags & SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER) !=
                trust) {
            fprintf(stderr, "trust=%d status=%04x size=%dx%d colors=%d "
                    "flags=%u\n", trust, status, width, height, ncolors, flags);
            goto done;
        }
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                painted = y < 6 || x >= 2;
                if (raw[y * width + x] != (painted ? 1 : 255) ||
                        mask[y * width + x] != (painted ? 255 : 0)) {
                    fprintf(stderr, "trust=%d pixel=%d,%d value=%u mask=%u\n",
                            trust, x, y, raw[y * width + x],
                            mask[y * width + x]);
                    goto done;
                }
            }
        }
        free(raw); free(mask); free(palette);
        raw = mask = palette = NULL;
    }
    ok = 1;
done:
    free(raw); free(mask); free(palette);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
