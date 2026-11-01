/* SPDX-License-Identifier: MIT */
/* smptec output FS skips retained-color error in gamma working units. */
#include <sixel.h>
int test_delta_output_residual(int, int);

int
test_delta_native_0133(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_residual(1, 1);
}
