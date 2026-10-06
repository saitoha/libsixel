/* SPDX-License-Identifier: MIT */
/* Verify delta p3 diffuse. */
#include <sixel.h>
int test_delta_p3_residual(int);

int
test_delta_native_0107(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_p3_residual(0);
}
