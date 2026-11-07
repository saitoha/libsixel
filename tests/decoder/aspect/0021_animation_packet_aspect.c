/* SPDX-License-Identifier: MIT */
/* A scaled repair paints its own rows over a unit-aspect animation canvas. */
#include <stdlib.h>
#include <string.h>
#include "src/sixel_decode_pixels.h"

int
test_decoder_aspect_0021(int argc, char **argv)
{
    static unsigned char const stream[] =
        "\033Pq\"1;1;1;3#1;2;100;0;0#1F\033\\"
        "\033P0;1q\"2;1;1;3#1;2;0;100;0#1@\033\\"
        "\033Pq\"3;1;1;3#1@\033\\";
    static unsigned char const first[] = {
        255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255
    };
    static unsigned char const second[] = {
        0, 255, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255
    };
    static unsigned char const third[] = {
        0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255
    };
    unsigned char const *expected[3];
    sixel_sequence_decoder_t *decoder;
    unsigned char const *rgba;
    SIXELSTATUS status;
    size_t end;
    int width;
    int height;
    int i;
    int ok;

    (void)argc;
    (void)argv;
    decoder = NULL;
    expected[0] = first;
    expected[1] = second;
    expected[2] = third;
    status = sixel_sequence_open(&decoder, stream, sizeof(stream) - 1U,
                                 1, NULL);
    ok = status == SIXEL_OK;
    for (i = 0; i < 3 && ok; ++i) {
        status = sixel_sequence_next(decoder, &rgba, &width, &height, &end);
        ok = status == SIXEL_OK && width == 1 && height == 3 &&
            memcmp(rgba, expected[i], sizeof(first)) == 0;
    }
    if (ok) {
        ok = sixel_sequence_next(decoder, &rgba, &width, &height, &end) ==
            SIXEL_FALSE && end == sizeof(stream) - 1U;
    }
    sixel_sequence_close(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
