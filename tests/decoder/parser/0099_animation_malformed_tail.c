/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "src/sixel_decode_pixels.h"

int
test_video_decode_0089(int argc, char **argv)
{
    static unsigned char const input[] =
        "\033Pq\"1;1;2;1#0;2;100;0;0#0@@\033\\"
        "\033P0;1q\"1;1;2;1#0@";
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
    ok = 0;
    status = sixel_sequence_open(&decoder, input, sizeof(input) - 1, 1, NULL);
    if (SIXEL_FAILED(status)) { goto done; }
    status = sixel_sequence_next(decoder, &rgba, &width, &height, &end);
    if (status != SIXEL_OK || width != 2 || height != 1) { goto done; }
    status = sixel_sequence_next(decoder, &rgba, &width, &height, &end);
    ok = SIXEL_FAILED(status) && rgba == NULL;
done:
    sixel_sequence_close(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
