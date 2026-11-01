/* SPDX-License-Identifier: MIT */
/* Verify smptec medoids native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(0, 1, 2, 0, NULL);
}
