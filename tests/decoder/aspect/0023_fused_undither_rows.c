/* SPDX-License-Identifier: MIT */
/* Fast4 must reconstruct at source resolution before repeating rows. A
 * nonuniform image detects a changed reconstruction neighborhood as well
 * as a missing expansion of the fused workers' separate output buffer. */
#include <stdlib.h>
#include <string.h>
#include "src/sixel_decode_pixels.h"

#define ASPECT_UNDITHER_PAINT \
    "#1;2;100;0;0#2;2;0;100;0#1~~~~~~~~~~~~~~~~" \
    "$#2TTTTTTTTTTTTTTTT-#1~~~~~~~~~~~~~~~~$#2TTTTTTTTTTTTTTTT\033\\"

int
test_decoder_aspect_0023(int argc, char **argv)
{
    static unsigned char original[] =
        "\033Pq\"1;1;16;12" ASPECT_UNDITHER_PAINT;
    static unsigned char scaled[] =
        "\033Pq\"2;1;16;24" ASPECT_UNDITHER_PAINT;
    unsigned char *reference;
    unsigned char *pixels;
    unsigned int flags;
    SIXELSTATUS status;
    int width;
    int height;
    int direct;
    int y;
    int stride;
    int ok;

    (void)argc;
    (void)argv;
    ok = 1;
    for (direct = 0; direct < 2 && ok; ++direct) {
        reference = pixels = NULL;
        stride = 16 * (direct ? 4 : 3);
        status = sixel_decode_kundither_fast4_with_options(original,
            sizeof(original) - 1U, direct, 0, 0U, &flags,
            &reference, &width, &height, NULL);
        ok = status == SIXEL_OK && width == 16 && height == 12;
        status = sixel_decode_kundither_fast4_with_options(scaled,
            sizeof(scaled) - 1U, direct, 0, 0U, &flags,
            &pixels, &width, &height, NULL);
        ok = ok && status == SIXEL_OK && width == 16 && height == 24;
        for (y = 0; y < 24 && ok; ++y) {
            ok = memcmp(pixels + y * stride,
                        reference + (y / 2) * stride, (size_t)stride) == 0;
        }
        free(pixels);
        free(reference);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
