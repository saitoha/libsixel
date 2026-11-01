/* SPDX-License-Identifier: MIT */
/* Verify delta p3 definition switch. */
#include <sixel.h>
int test_delta_p3_identity(int);

int
test_delta_native_0117(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_identity(0);
}
