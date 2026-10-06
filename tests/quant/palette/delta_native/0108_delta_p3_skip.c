/* SPDX-License-Identifier: MIT */
/* Verify delta p3 skip. */
#include <sixel.h>
int test_delta_p3_residual(int);

int
test_delta_native_0108(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_residual(1);
}
