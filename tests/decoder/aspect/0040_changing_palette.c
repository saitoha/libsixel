/* SPDX-License-Identifier: MIT */
/* Definitions are mapped once; direct output keeps paint-time colors while
 * indexed output reports the redefinition and exposes the final palette. */
#include <stdlib.h>
#include <string.h>
#include "src/sixel_decode_pixels.h"

static void
aspect_swap_red_blue(void *context, unsigned char rgb[3], int is_default)
{
    unsigned char red;

    (void)context;
    (void)is_default;
    red = rgb[0];
    rgb[0] = rgb[2];
    rgb[2] = red;
}

int
test_decoder_aspect_0040(int argc, char **argv)
{
    static unsigned char stream[] =
        "\033Pq\"1;1;2;4#1;2;100;0;0@\"2;1;2;4#1;2;0;100;0A\033\\";
    static unsigned char const expected[] = {
        0, 0, 255, 255, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 255, 0, 255,
        0, 0, 0, 0, 0, 255, 0, 255
    };
    sixel_palette_transform_t transform;
    unsigned char *pixels;
    unsigned char *palette;
    unsigned char *mask;
    unsigned int flags;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int ok;

    (void)argc;
    (void)argv;
    transform.context = NULL;
    transform.map_rgb = aspect_swap_red_blue;
    pixels = palette = mask = NULL;
    status = sixel_decode_direct_mapped(stream, sizeof(stream) - 1, 0U,
        &transform, 0, NULL, 0, &pixels, &width, &height, &palette,
        &ncolors, &flags, NULL);
    ok = status == SIXEL_OK && width == 2 && height == 4 &&
        flags == SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED &&
        memcmp(pixels, expected, sizeof(expected)) == 0;
    free(pixels);
    free(palette);
    pixels = palette = NULL;
    status = sixel_decode_raw_mapped(stream, sizeof(stream) - 1, 0U,
        &transform, &pixels, &mask, &width, &height, &palette,
        &ncolors, &flags, NULL);
    ok = ok && status == SIXEL_OK && width == 2 && height == 4 &&
        flags == SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED &&
        pixels[0] == 1 && pixels[5] == 1 && pixels[7] == 1 &&
        mask[0] == 255 && mask[1] == 0 && mask[5] == 255 &&
        palette[3] == 0 && palette[4] == 255 && palette[5] == 0;
    free(pixels);
    free(palette);
    free(mask);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
