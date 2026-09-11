/* SPDX-License-Identifier: MIT */
/* Check the CLI offset against an independent, four-color source raster. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int
test_or_offset_cli(int argc, char **argv)
{
    unsigned char const colors[4][3] = {
        { 255, 0, 0 }, { 0, 255, 0 },
        { 0, 0, 255 }, { 255, 255, 255 }
    };
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    int width;
    int height;
    int x;
    int y;
    int ok;

    allocator = NULL;
    pixels = NULL;
    ok = 0;
    if (argc != 2 || strstr(argv[1], "P7;5q") == NULL) {
        return EXIT_FAILURE;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                                         NULL, NULL))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_decode_direct((unsigned char *)argv[1],
            (int)strlen(argv[1]), &pixels, &width, &height, allocator)) ||
            width != 5 || height != 7) {
        goto end;
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (pixels[(y * width + x) * 4 + 3] != 255) {
                goto end;
            }
            if (x >= 3 && y >= 5) {
                if (memcmp(pixels + (y * width + x) * 4,
                           colors[(y - 5) * 2 + x - 3], 3) != 0) {
                    goto end;
                }
            } else if (memcmp(pixels + (y * width + x) * 4,
                              "\0\0\0", 3) != 0) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, pixels);
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
