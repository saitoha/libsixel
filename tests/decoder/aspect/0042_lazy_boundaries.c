/* SPDX-License-Identifier: MIT */
/* Lazy detection stops at ST, and never applies ESC-interrupted or unfinished
 * DECGRA parameters. Check decoded samples, including blank/cursor/repeat
 * drawing bytes, rather than a separate classification of the input. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../_support/processing/decoder/aspect_test_common.h"

int
test_decoder_aspect_0042(int argc, char **argv)
{
    static struct {
        char const *body;
        int width;
        int height;
        char const *pixels;
    } const cases[] = {
        { "\"5;2@\"3;1@\033\\", 2, 3, "RRRRRR" },
        { "@\"1;2@\"5;4@\033\\", 3, 1, "RRR" },
        { "@\033\\\033Pq\"2;1@\033\\", 1, 1, "R" },
        { "\"2;1@\033\\\033Pq\"3;1@\033\\", 1, 2, "RR" },
        { "\"2;1@\"3;1\033\\", 1, 2, "RR" },
        { "\"2;1@\"3;1", 1, 2, "RR" },
        { "@\"2;1@\033\\", 2, 2, "RR.R" },
        { "?\"2;1@\033\\", 2, 2, ".R.R" },
        { "!1\"2;1@\033\\", 1, 2, "RR" },
        { "$\"2;1@\033\\", 1, 2, "RR" },
        { "-\"2;1@\033\\", 1, 8, "......RR" },
        { "\"2;1@\"1;1@\033\\", 2, 2, "RRR." }
    };
    static char const head[] = "\033P9q#1;2;100;0;0";
    char stream[128];
    size_t i;

    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        memcpy(stream, head, sizeof(head) - 1);
        memcpy(stream + sizeof(head) - 1, cases[i].body,
               strlen(cases[i].body) + 1);
        if (test_decoder_aspect_pixels(stream, cases[i].width,
                cases[i].height, cases[i].pixels, 0U, 0U) != EXIT_SUCCESS) {
            fprintf(stderr, "lazy boundary case %zu failed\n", i);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
