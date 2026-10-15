/* SPDX-License-Identifier: MIT */
/* Direct materialization uses the old palette; raw uses the final palette. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/sixel_decode_pixels.h"

int
test_decoder_0095(int argc, char **argv)
{
    unsigned char input[] =
        "\033Pq\"1;1;3;6#1;2;100;0;0~#1;2;0;100;0~\033\\";
    unsigned char *raw;
    unsigned char *mask;
    unsigned char *palette;
    sixel_decode_result_t result;
    unsigned int flags;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int y;
    int ok;
    static unsigned char const row[] = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 0, 0
    };

    (void)argc;
    (void)argv;
    raw = mask = palette = NULL;
    memset(&result, 0, sizeof(result));
    ok = 0;
    status = sixel_decode_pixels(input, sizeof(input) - 1, NULL,
                                 &result, NULL);
    if (SIXEL_FAILED(status) || result.width != 3 || result.height != 6 ||
            !(result.flags & SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED)) {
        goto done;
    }
    for (y = 0; y < 6; ++y) {
        if (memcmp(result.pixels + y * 12, row, sizeof(row))) { goto done; }
    }
    status = sixel_decode_raw_with_options_mask(input, sizeof(input) - 1,
        0, &raw, &mask, &width, &height, &palette, &ncolors, &flags, NULL);
    if (SIXEL_FAILED(status) || width != 3 || height != 6 || ncolors != 2 ||
            !(flags & SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED) ||
            palette[3] != 0 || palette[4] != 255 || palette[5] != 0) {
        goto done;
    }
    for (y = 0; y < 6; ++y) {
        if (raw[y * 3] != 1 || raw[y * 3 + 1] != 1 ||
                raw[y * 3 + 2] != 255 || mask[y * 3] != 255 ||
                mask[y * 3 + 1] != 255 || mask[y * 3 + 2] != 0) {
            goto done;
        }
    }
    ok = 1;
done:
    free(result.pixels); free(raw); free(mask); free(palette);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
