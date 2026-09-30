/* SPDX-License-Identifier: MIT */
/* Verify output reuse native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0027(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(2, 0, 0, 3, NULL);
}
