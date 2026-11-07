/* SPDX-License-Identifier: MIT */
/* Reject scaled height limit before allocating a scaled result. */
#include <stdlib.h>
#include <sixel.h>

int
test_decoder_aspect_0019(int argc, char **argv)
{
    static unsigned char stream[] = "\033Pq\"600000;1#1A\033\\";
    unsigned char *pixels;
    unsigned char *palette;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;

    (void)argc;
    (void)argv;
    pixels = palette = NULL;
    status = sixel_decode_raw(stream, sizeof(stream) - 1U, &pixels,
        &width, &height, &palette, &ncolors, NULL);
    free(pixels);
    free(palette);
    return status == SIXEL_BAD_INPUT ? EXIT_SUCCESS : EXIT_FAILURE;
}
