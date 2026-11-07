/* SPDX-License-Identifier: MIT */
/* OR retains high index bits and resolves them against the final palette. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

SIXELSTATUS sixel_decode_wide(unsigned char *, int, unsigned short **,
    int *, int *, unsigned char **, int *, sixel_allocator_t *);

int
test_decoder_aspect_0041(int argc, char **argv)
{
    static unsigned char stream[] =
        "\033P0;5q\"1;1;1;2#1;2;0;100;0#256@"
        "$\"2;1;1;2#1@#257;2;0;0;100$\033\\";
    static unsigned char const expected[] = {
        0, 0, 255, 255, 0, 255, 0, 255
    };
    unsigned short *indices;
    unsigned char *pixels;
    unsigned char *palette;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int ok;

    (void)argc;
    (void)argv;
    indices = NULL;
    pixels = palette = NULL;
    status = sixel_decode_wide(stream, sizeof(stream) - 1, &indices,
        &width, &height, &palette, &ncolors, NULL);
    ok = status == SIXEL_OK && width == 1 && height == 2 &&
        indices[0] == 257 && indices[1] == 1 && ncolors >= 258;
    free(indices);
    free(palette);
    palette = NULL;
    status = sixel_decode_direct(stream, sizeof(stream) - 1, &pixels,
                                 &width, &height, NULL);
    ok = ok && status == SIXEL_OK && width == 1 && height == 2 &&
        memcmp(pixels, expected, sizeof(expected)) == 0;
    free(pixels);
    free(palette);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
