/* SPDX-License-Identifier: MIT */
/* Apply the last head scale to subsequent paint. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/sixel_decode_pixels.h"
#include "src/decoder-parallel.h"
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0007(int argc, char **argv)
{
    static char const stream[] =
        "\033Pq\"2;1;2;4\"3;1;2;6" ASPECT_COLORS ASPECT_PAINT "\033\\";

    static char const retained_pv[] =
        "\033Pq\"1;1;2;6\"2;1" ASPECT_COLORS "#1~~\033\\";

    static char const head[] =
        "\033Pq\"1;1;128;96\"2;1;64" ASPECT_COLORS "#1";
    char large[1200];
    char expected[64 * 192 + 1];
    char *p;
    int band;
    int budget;
    int ok;

    (void)argc;
    (void)argv;
    if (test_decoder_aspect_pixels(stream, 2, 6, "R.R.R..G.G.G",
            0U, 0U) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    /* A head scale change without a new Pv must recompute source bounds.
     * Full sixels exceed the retained six device rows at the new scale. */
    if (test_decoder_aspect_pixels(retained_pv, 2, 12,
            "RRRRRRRRRRRRRRRRRRRRRRRR", 0U,
            SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    /* A large header-only scale change must give workers the new source
     * bounds, including a smaller Ph and an omitted Pv. */
    p = large;
    memcpy(p, head, sizeof(head) - 1);
    p += sizeof(head) - 1;
    for (band = 0; band < 16; ++band) {
        memset(p, '~', 64);
        p += 64;
        *p++ = '-';
    }
    memcpy(p, "\033\\", 3);
    memset(expected, 'R', sizeof(expected) - 1);
    expected[sizeof(expected) - 1] = '\0';
    ok = 1;
    for (budget = 0; budget < 2 && ok; ++budget) {
        ok = sixel_decoder_parallel_override_threads(budget ? "4" : "1") ==
            SIXEL_OK && test_decoder_aspect_pixels(large, 64, 192, expected,
                0U, SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER) ==
            EXIT_SUCCESS;
        ok = ok && test_decoder_aspect_pixels(large, 64, 96, expected,
            SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE,
            SIXEL_DECODE_PIXELS_RESULT_PAINT_OUTSIDE_RASTER |
            SIXEL_DECODE_PIXELS_RESULT_CLIPPED_TO_RASTER) == EXIT_SUCCESS;
    }
    (void)sixel_decoder_parallel_override_threads("1");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
