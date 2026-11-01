/* SPDX-License-Identifier: MIT */
/* Verify linear fixed map native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0025(int argc, char **argv)
{
    return test_output_native(0, 0, 0, 5, argc == 2 ? argv[1] : NULL);
}
