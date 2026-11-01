/* SPDX-License-Identifier: MIT */
/* Register zero can carry 257 distinct retained colors over many passes. */
#include <stdio.h>
#include <string.h>
#include "../../_support/processing/decoder/multi_pass_test_common.h"

int test_sequence_0084(int argc, char **argv)
{
    char stream[20000];
    char skip[32];
    unsigned char expected[257 * 4];
    size_t used;
    int i;
    int n;

    (void)argc;
    (void)argv;
    used = 0U;
    for (i = 0; i < 257; ++i) {
        skip[0] = '\0';
        if (i != 0) {
            snprintf(skip, sizeof(skip), "!%d?", i);
        }
        n = snprintf(stream + used, sizeof(stream) - used,
            "\033P0;1q\"1;1;257;1#0;2;%d;%d;0%s@\033\\",
            i % 101, i / 101, skip);
        if (n < 0 || (size_t)n >= sizeof(stream) - used) {
            return 1;
        }
        used += (size_t)n;
        /* Independent integer scaling of the transmitted percentages. */
        expected[i * 4] = (unsigned char)(((i % 101) * 255 + 50) / 100);
        expected[i * 4 + 1] =
            (unsigned char)(((i / 101) * 255 + 50) / 100);
        expected[i * 4 + 2] = 0;
        expected[i * 4 + 3] = 255;
    }
    return test_multi_pass_capture(stream, used, expected, 257, 1, 257U, "");
}
