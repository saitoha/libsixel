/* SPDX-License-Identifier: MIT */
/* Inspect actual SIXEL paint coverage and wire-rounded source RGB. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/sixel_decode_pixels.h"
#include "tests/data/color-numeric/palette-trns.h"

int
test_loader_0191_png_palette_paint(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *mask;
    unsigned char *palette;
    SIXELSTATUS status;
    unsigned int flags;
    int width;
    int height;
    int ncolors;
    int variant;
    int p;
    int c;
    int index;
    int expected;
    int result;

    if (argc != 3) {
        return 1;
    }
    variant = atoi(argv[1]);
    if (variant < 0 || variant > 3 || strlen(argv[2]) > 65536) {
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
    status = sixel_decode_raw_with_options_mask((unsigned char *)argv[2],
        (int)strlen(argv[2]), SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE,
        &pixels, &mask, &width, &height, &palette, &ncolors, &flags, allocator);
    if (SIXEL_FAILED(status) || width != 4 || height != 2 ||
        pixels == NULL || mask == NULL || palette == NULL || flags != 0) {
        fprintf(stderr, "palette SIXEL: invalid raster or decode status\n");
        goto cleanup;
    }
    for (p = 0; p < 8; ++p) {
        index = color_pal_indices[p];
        expected = color_pal_alpha[variant][index] == 0 ? 0 : 255;
        if (mask[p] != expected || (expected != 0 && pixels[p] >= ncolors)) {
            fprintf(stderr, "paint[%d]=%u expected=%d\n", p, mask[p], expected);
            goto cleanup;
        }
        if (expected == 0) {
            continue;
        }
        for (c = 0; c < 3; ++c) {
            /* The encoder's nearest 0..100 RGB wire coordinate, then the
             * decoder's nearest RGB8 value. Hidden RGB has no paint value. */
            expected = (color_pal_rgb[index * 3 + c] * 100 + 127) / 255;
            expected = (expected * 255 + 50) / 100;
            if (palette[pixels[p] * 3 + c] != expected) {
                fprintf(stderr, "paint RGB[%d][%d]=%u expected=%d\n", p, c,
                        palette[pixels[p] * 3 + c], expected);
                goto cleanup;
            }
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
