/* SPDX-License-Identifier: MIT */
/* Fixed delta: fixed fs skip. */
#include <stddef.h>
int test_delta_fixed_residual(int);

int
test_delta_native_0049(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_fixed_residual(1);
}
