/* SPDX-License-Identifier: MIT */
/* Verify empty capture. */
#include "../../_support/processing/decoder/multi_pass_test_common.h"

int test_sequence_0078(int argc, char **argv)
{
    static char const stream[] =
        " \012";

    (void)argc;
    (void)argv;
    return test_multi_pass_capture(stream, sizeof(stream) - 1U,
        NULL, 0, 0, 0U, "");
}
