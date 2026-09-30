/* SPDX-License-Identifier: MIT */
/* Verify smptec din99d native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(4, 1, 0, 0, NULL);
}
