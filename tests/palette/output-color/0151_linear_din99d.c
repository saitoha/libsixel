/* SPDX-License-Identifier: MIT */
/* Verify linear din99d native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(4, 0, 0, 0, NULL);
}
