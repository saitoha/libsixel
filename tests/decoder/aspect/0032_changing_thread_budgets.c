/* SPDX-License-Identifier: MIT */
/* A full-width multi-band payload would request parallel fast-path work.
 * Compare both budgets to exact output rows across all four decode APIs. */
#include <stdlib.h>
#include <string.h>
#include "src/decoder-parallel.h"
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0032(int argc, char **argv)
{
    static char const header[] =
        "\033Pq\"1;1;512;144" ASPECT_COLORS;
    static char const change[] = "\"2;1;512;144";
    char stream[9000];
    char expected[512 * 144 + 1];
    char *p;
    int band;
    int y;
    int ok;

    (void)argc;
    (void)argv;
    p = stream;
    memcpy(p, header, sizeof(header) - 1);
    p += sizeof(header) - 1;
    for (band = 0; band < 16; ++band) {
        if (band == 8) {
            memcpy(p, change, sizeof(change) - 1);
            p += sizeof(change) - 1;
        }
        *p++ = '#';
        *p++ = (char)('1' + band % 2);
        memset(p, '~', 512);
        p += 512;
        *p++ = '-';
    }
    memcpy(p, "\033\\", 3);
    for (y = 0; y < 144; ++y) {
        band = y < 48 ? y / 6 : 8 + (y - 48) / 12;
        memset(expected + y * 512, band % 2 == 0 ? 'R' : 'G', 512);
    }
    expected[512 * 144] = '\0';
    ok = sixel_decoder_parallel_override_threads("1") == SIXEL_OK &&
        test_decoder_aspect_pixels(stream, 512, 144, expected,
                                   0U, 0U) == EXIT_SUCCESS;
    ok = ok && sixel_decoder_parallel_override_threads("4") == SIXEL_OK &&
        test_decoder_aspect_pixels(stream, 512, 144, expected,
                                   0U, 0U) == EXIT_SUCCESS;
    (void)sixel_decoder_parallel_override_threads("1");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
