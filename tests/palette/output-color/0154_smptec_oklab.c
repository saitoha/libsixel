/* SPDX-License-Identifier: MIT */
/* Verify smptec oklab native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(2, 1, 0, 0, NULL);
}
