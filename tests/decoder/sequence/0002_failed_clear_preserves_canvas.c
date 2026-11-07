/* SPDX-License-Identifier: MIT */
/* A failed P2=0 packet must not clear the last published animation frame. */
#include <stdlib.h>
#include <string.h>
#include "src/sixel_decode_pixels.h"

int
test_decoder_sequence_0002(int argc, char **argv)
{
    static unsigned char const input[] =
        "\033P0;1q\"1;1;1;1#0;2;100;0;0@\033\\"
        "\033P0;0;2147483647q\"1;1;1;1#0@\033\\";
    static unsigned char const expected[] = {255, 0, 0, 255};
    sixel_sequence_decoder_t *decoder;
    unsigned char const *rgba;
    unsigned char const *published;
    SIXELSTATUS status;
    size_t end;
    size_t published_end;
    int width;
    int height;
    int ok;

    (void)argc;
    (void)argv;
    decoder = NULL;
    ok = 0;
    status = sixel_sequence_open(&decoder, input, sizeof(input) - 1, 1, NULL);
    if (status != SIXEL_OK) { goto done; }
    status = sixel_sequence_next(decoder, &rgba, &width, &height, &end);
    if (status != SIXEL_OK || width != 1 || height != 1 || rgba == NULL) {
        goto done;
    }
    if (memcmp(rgba, expected, sizeof(expected)) != 0) { goto done; }
    published = rgba;
    published_end = end;
    status = sixel_sequence_next(decoder, &rgba, &width, &height, &end);
    if (status != SIXEL_BAD_INTEGER_OVERFLOW || rgba != NULL ||
            end != published_end ||
            memcmp(published, expected, sizeof(expected)) != 0) {
        goto done;
    }
    /* Failure is terminal, and a retry cannot change the borrowed canvas. */
    status = sixel_sequence_next(decoder, &rgba, &width, &height, &end);
    ok = status == SIXEL_BAD_INPUT && rgba == NULL &&
        end == published_end &&
        memcmp(published, expected, sizeof(expected)) == 0;
done:
    sixel_sequence_close(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
