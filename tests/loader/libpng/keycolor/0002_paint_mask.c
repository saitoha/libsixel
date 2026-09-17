/* SPDX-License-Identifier: MIT */
/* Independent source pixels define paint coverage, not encoder output. */
#include <stdio.h>
#include <string.h>
#include "src/sixel_decode_pixels.h"
#include "tests/data/color-numeric/png-keycolor.h"

int
test_png_key_0002(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *mask;
    unsigned char *palette;
    unsigned char const *rgb;
    SIXELSTATUS status;
    unsigned int flags;
    int width;
    int height;
    int ncolors;
    int i;
    int expected;
    int result;

    /* The controlled CLI stream requests clear (P2=0). Pixels left unpainted
     * are measured separately from black palette entries or background RGB. */
    if (argc != 2 || strlen(argv[1]) > 65536 ||
        strncmp(argv[1], "\033P0;0q", 6) != 0) {
        return 1;
    }
    allocator = NULL;
    pixels = NULL;
    mask = NULL;
    palette = NULL;
    result = 1;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }
    status = sixel_decode_raw_with_options_mask((unsigned char *)argv[1],
        (int)strlen(argv[1]), SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE,
        &pixels, &mask, &width, &height, &palette, &ncolors, &flags, allocator);
    if (SIXEL_FAILED(status) || width != 32 || height != 32 ||
        pixels == NULL || mask == NULL || palette == NULL || flags != 0) {
        goto cleanup;
    }
    for (i = 0; i < 1024; ++i) {
        rgb = color_key_rgb[i];
        expected = rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255 ? 0 : 255;
        if (mask[i] != expected || (expected && pixels[i] >= ncolors)) {
            fprintf(stderr, "keycolor paint mask mismatch at %d\n", i);
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, mask);
    sixel_allocator_free(allocator, palette);
    sixel_allocator_unref(allocator);
    return result;
}
