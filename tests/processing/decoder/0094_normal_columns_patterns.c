/* SPDX-License-Identifier: MIT */
/* All six-bit patterns preserve overwrite order, zero indexes and holes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/sixel_decode_pixels.h"

int
test_decoder_0094(int argc, char **argv)
{
    unsigned char input[256];
    unsigned char *pixels;
    unsigned char *mask;
    unsigned char *palette;
    unsigned char *rgba;
    unsigned int flags;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int length;
    int keep;
    int x;
    int y;
    int painted;
    int expected;
    int ok;

    (void)argc;
    (void)argv;
    pixels = mask = palette = rgba = NULL;
    ok = 0;
    length = snprintf((char *)input, sizeof(input),
        "\033P0;1q\"1;1;64;12#0;2;0;100;0#1;2;100;0;0!64~$#0");
    for (x = 0; x < 64; ++x) { input[length++] = (unsigned char)('?' + x); }
    input[length++] = '-';
    for (x = 0; x < 64; ++x) { input[length++] = (unsigned char)('?' + x); }
    input[length++] = 0x1b;
    input[length++] = '\\';
    for (keep = 0; keep < 2; ++keep) {
        status = sixel_decode_raw_with_options_mask(input, length, 0,
            &pixels, keep ? &mask : NULL, &width, &height, &palette,
            &ncolors, &flags, NULL);
        if (SIXEL_FAILED(status) || width != 64 || height != 12 ||
                ncolors != 2 || flags != 0) { goto done; }
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                painted = y < 6 || (x & (1 << (y % 6)));
                expected = (x & (1 << (y % 6))) ? 0 : (y < 6 ? 1 : 255);
                if (pixels[y * width + x] != expected ||
                        (keep && mask[y * width + x] !=
                         (painted ? 255 : 0))) { goto done; }
            }
        }
        free(pixels); free(mask); free(palette);
        pixels = mask = palette = NULL;
    }
    status = sixel_decode_direct(input, length, &rgba, &width, &height, NULL);
    if (SIXEL_FAILED(status) || width != 64 || height != 12) { goto done; }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            expected = (x & (1 << (y % 6))) ? 0 : (y < 6 ? 1 : 255);
            pixels = rgba + (y * width + x) * 4;
            if (pixels[0] != (expected == 1 ? 255 : 0) ||
                    pixels[1] != (expected == 0 ? 255 : 0) ||
                    pixels[2] != 0 ||
                    pixels[3] != (expected == 255 ? 0 : 255)) {
                pixels = NULL;
                goto done;
            }
        }
    }
    pixels = NULL;
    ok = 1;
done:
    free(pixels); free(mask); free(palette); free(rgba);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
