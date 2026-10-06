/* SPDX-License-Identifier: MIT */
/* Verify delta p3 threshold. */
#include <sixel.h>
int test_delta_p3_threshold(int);

int
test_delta_native_0106(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_threshold(0);
}
