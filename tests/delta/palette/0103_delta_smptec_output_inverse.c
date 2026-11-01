/* SPDX-License-Identifier: MIT */
/* Independent smptec output inverse preserves extended coordinates. */
#include <sixel.h>
int test_delta_output_inverse(int);

int
test_delta_native_0130(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_output_inverse(1);
}
