/* SPDX-License-Identifier: MIT */
/* The raw decoder scales wire percentages without choosing an RGB space. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int test_pmap_0053(int argc, char **argv)
{
    unsigned char stream[4096];
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    SIXELSTATUS status;
    size_t used;
    int count;
    int tone;
    int y;
    int expected;
    int ok;

    (void)argc;
    (void)argv;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    memcpy(stream, "\033Pq", 3u);
    used = 3u;
    for (tone = 0; tone <= 100; ++tone) {
        count = snprintf((char *)stream + used, sizeof(stream) - used,
                         "#%d;2;%d;%d;%d~", tone, tone, tone, tone);
        if (count < 0 || (size_t)count >= sizeof(stream) - used) {
            return EXIT_FAILURE;
        }
        used += (size_t)count;
    }
    memcpy(stream + used, "\033\\", 2u);
    used += 2u;
    status = sixel_decode_pixels(stream, used, &options, &result, NULL);
    ok = SIXEL_SUCCEEDED(status) && result.width == 101 && result.height == 6
        && result.pixelformat == SIXEL_PIXELFORMAT_RGB888;
    if (ok) {
        for (y = 0; y < 6; ++y) {
            for (tone = 0; tone <= 100; ++tone) {
                /* Independent integer rounding of the wire fraction. */
                expected = (tone * 255 + 50) / 100;
                used = ((size_t)y * 101u + (size_t)tone) * 3u;
                if (result.pixels[used] != expected ||
                    result.pixels[used + 1u] != expected ||
                    result.pixels[used + 2u] != expected) {
                    ok = 0;
                }
            }
        }
    }
    free(result.pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
