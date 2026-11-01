/* SPDX-License-Identifier: MIT */
/* Preserve legacy offsets and fallback for data after an embedded ST. This
 * deliberately compares at the same budget: legacy malformed input can
 * already differ between serial and parallel parsing. */
#include <stdio.h>
#include <stdlib.h>
#include <sixel.h>
#include "src/decoder-parallel.h"

int
test_decoder_0103(int argc, char **argv)
{
    unsigned char input[] =
        "\033P0;1q\"1;1;8;192#1;2;100;0;0!8~-\033\\!8~-!8~-\033\\";
    char const *budgets[] = { "1", "2", "8", "12" };
    unsigned char *pixels;
    unsigned char *palette;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int budget;
    int x;
    int y;
    int expected;
    int ok;

    (void)argc;
    (void)argv;
    pixels = palette = NULL;
    ok = 0;
    for (budget = 0; budget < 4; ++budget) {
        status = sixel_decoder_parallel_override_threads(budgets[budget]);
        if (SIXEL_FAILED(status)) { goto end; }
        status = sixel_decode_raw(input, sizeof(input) - 1, &pixels,
            &width, &height, &palette, &ncolors, NULL);
        if (SIXEL_FAILED(status) || width != 8 || height != 192 ||
                ncolors != 2) { goto end; }
        for (y = 0; y < height; ++y) {
            expected = y < 6 || (budget == 1 && y >= 12 && y < 18) ?
                1 : 255;
            for (x = 0; x < width; ++x) {
                if (pixels[y * width + x] != expected) {
                    fprintf(stderr, "budget=%s pixel=%d,%d\n",
                            budgets[budget], x, y);
                    goto end;
                }
            }
        }
        free(pixels); free(palette);
        pixels = palette = NULL;
    }
    ok = 1;
end:
    (void)sixel_decoder_parallel_override_threads("1");
    free(pixels); free(palette);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
