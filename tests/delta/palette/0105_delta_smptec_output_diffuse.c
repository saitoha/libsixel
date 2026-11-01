/* SPDX-License-Identifier: MIT */
/* smptec output FS propagates retained-color error in gamma working
 * units. */
#include <sixel.h>
int test_delta_output_residual(int, int);

int
test_delta_native_0132(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_residual(1, 0);
}
