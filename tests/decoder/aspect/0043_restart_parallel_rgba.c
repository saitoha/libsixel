/* SPDX-License-Identifier: MIT */
/* A late DECGRA follows eight full bands, after workers have parsed private
 * columns. Restart must restore the default palette: redefining register 1
 * before the restart must not recolor the earlier direct RGBA paint. */
#include <stdlib.h>
#include <string.h>
#include "src/decoder-parallel.h"
#include "src/sixel_decode_pixels.h"

int
test_decoder_aspect_0043(int argc, char **argv)
{
    static char const head[] = "\033Pq\"2;1;512;144#1";
    static char const redefine[] = "#1;2;100;0;0";
    static char const change[] = "\"1;1;512;144";
    unsigned char stream[9000];
    unsigned char *p;
    unsigned char *pixels;
    unsigned char *palette;
    SIXELSTATUS status;
    unsigned int flags;
    int width;
    int height;
    int ncolors;
    int band;
    int budget;
    int pos;
    int ok;

    (void)argc;
    (void)argv;
    p = stream;
    memcpy(p, head, sizeof(head) - 1);
    p += sizeof(head) - 1;
    for (band = 0; band < 16; ++band) {
        if (band == 4) {
            memcpy(p, redefine, sizeof(redefine) - 1);
            p += sizeof(redefine) - 1;
        }
        if (band == 8) {
            memcpy(p, change, sizeof(change) - 1);
            p += sizeof(change) - 1;
        }
        memset(p, '~', 512);
        p += 512;
        *p++ = '-';
    }
    memcpy(p, "\033\\", 2);
    p += 2;
    ok = 1;
    for (budget = 0; budget < 2 && ok; ++budget) {
        status = sixel_decoder_parallel_override_threads(budget ? "4" : "1");
        ok = status == SIXEL_OK;
        pixels = palette = NULL;
        status = sixel_decode_direct_with_options(stream, (int)(p - stream),
            0U, &pixels, &width, &height, &palette, &ncolors, &flags, NULL);
        ok = ok && status == SIXEL_OK && width == 512 && height == 144 &&
            flags == SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED;
        for (pos = 0; pos < 512 * 144 && ok; ++pos) {
            ok = pixels[pos * 4] == (pos < 512 * 48 ? 51 : 255) &&
                pixels[pos * 4 + 1] == (pos < 512 * 48 ? 51 : 0) &&
                pixels[pos * 4 + 2] == (pos < 512 * 48 ? 204 : 0) &&
                pixels[pos * 4 + 3] == 255;
        }
        free(pixels);
        free(palette);
    }
    (void)sixel_decoder_parallel_override_threads("1");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
