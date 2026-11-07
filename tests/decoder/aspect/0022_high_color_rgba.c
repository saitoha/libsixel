/* SPDX-License-Identifier: MIT */
/* Wide color registers force the ordinary direct-color painter. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int
test_decoder_aspect_0022(int argc, char **argv)
{
    static unsigned char const stream[] =
        "\033Pq\"2;1;1;4#300;2;100;0;0#300@\033\\";
    static unsigned char const expected[] = {
        255, 0, 0, 255, 255, 0, 0, 255,
        0, 0, 0, 0, 0, 0, 0, 0
    };
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    SIXELSTATUS status;
    int ok;

    (void)argc;
    (void)argv;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    status = sixel_decode_pixels(stream, sizeof(stream) - 1U,
                                &options, &result, NULL);
    ok = status == SIXEL_OK && result.width == 1 && result.height == 4 &&
        memcmp(result.pixels, expected, sizeof(expected)) == 0;
    free(result.pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
