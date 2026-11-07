/* SPDX-License-Identifier: MIT */
/* A mid-band repair changes later columns, preserving the prior frame. */
#include <stdlib.h>
#include <string.h>
#include "src/sixel_decode_pixels.h"

int
test_decoder_aspect_0035(int argc, char **argv)
{
    static unsigned char const stream[] =
        "\033Pq\"1;1;2;4#1;2;100;0;0#1!2N\033\\"
        "\033P0;1q\"1;1;2;4#2;2;0;100;0#2@\"2;1;2;4A\033\\";
    static unsigned char const expected[] = {
        0, 255, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 0, 255, 0, 255,
        255, 0, 0, 255, 0, 255, 0, 255
    };
    sixel_sequence_decoder_t *decoder;
    unsigned char const *rgba;
    SIXELSTATUS status;
    size_t end;
    int width;
    int height;
    int ok;

    (void)argc;
    (void)argv;
    decoder = NULL;
    status = sixel_sequence_open(&decoder, stream, sizeof(stream) - 1,
                                 1, NULL);
    ok = status == SIXEL_OK;
    if (ok) {
        ok = sixel_sequence_next(decoder, &rgba, &width, &height, &end) ==
            SIXEL_OK;
    }
    if (ok) {
        status = sixel_sequence_next(decoder, &rgba, &width, &height, &end);
        ok = status == SIXEL_OK && width == 2 && height == 4 &&
            memcmp(rgba, expected, sizeof(expected)) == 0;
    }
    if (ok) {
        ok = sixel_sequence_next(decoder, &rgba, &width, &height, &end) ==
            SIXEL_FALSE && end == sizeof(stream) - 1;
    }
    sixel_sequence_close(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
