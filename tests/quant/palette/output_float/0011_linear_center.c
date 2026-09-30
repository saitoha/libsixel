/* SPDX-License-Identifier: MIT */
/* Verify linear center native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(0, 0, 1, 0, NULL);
}
