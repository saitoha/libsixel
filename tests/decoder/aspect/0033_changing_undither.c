/* SPDX-License-Identifier: MIT */
/* Fast4 reconstructs the output-coordinate indexed image, without fusion
 * or a second row expansion. Check RGB and RGBA with both thread budgets. */
#include <stdlib.h>
#include <string.h>
#include "src/decoder.h"
#include "src/decoder-parallel.h"
#include "src/sixel_decode_pixels.h"

int
test_decoder_aspect_0033(int argc, char **argv)
{
    static unsigned char stream[] =
        "\033Pq\"1;1;16;18#1;2;100;0;0#2;2;0;100;0"
        "#1!16~$#2!16T-\"2;1;16;18#1!16~$#2!16T\033\\";
    unsigned char *indexed;
    unsigned char *palette;
    unsigned char *reference;
    unsigned char *pixels;
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    unsigned int flags;
    int width;
    int height;
    int ncolors;
    int direct;
    int budget;
    int pos;
    int ok;

    (void)argc;
    (void)argv;
    indexed = palette = reference = pixels = NULL;
    allocator = NULL;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    ok = status == SIXEL_OK;
    if (ok) {
        status = sixel_decode_raw(stream, sizeof(stream) - 1,
            &indexed, &width, &height, &palette, &ncolors, allocator);
        ok = status == SIXEL_OK && width == 16 && height == 18;
    }
    if (ok) {
        status = sixel_dequantize_k_undither_fast4(indexed, width, height,
            palette, ncolors, 0, allocator, &reference);
        ok = status == SIXEL_OK;
    }
    for (budget = 0; budget < 2 && ok; ++budget) {
        status = sixel_decoder_parallel_override_threads(budget ? "4" : "1");
        ok = status == SIXEL_OK;
        for (direct = 0; direct < 2 && ok; ++direct) {
            status = sixel_decode_kundither_fast4_with_options(stream,
                sizeof(stream) - 1, direct, 0, 0U, &flags,
                &pixels, &width, &height, allocator);
            ok = status == SIXEL_OK && width == 16 && height == 18 &&
                flags == 0U;
            for (pos = 0; pos < 16 * 18 && ok; ++pos) {
                ok = memcmp(pixels + pos * (direct ? 4 : 3),
                            reference + pos * 3, 3U) == 0;
                if (direct && ok) {
                    ok = pixels[pos * 4 + 3] == 255;
                }
            }
            free(pixels);
            pixels = NULL;
        }
    }
    (void)sixel_decoder_parallel_override_threads("1");
    free(reference);
    free(indexed);
    free(palette);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
