/* SPDX-License-Identifier: MIT */
/* Verify linear zero snap native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(0, 0, 0, 2, NULL);
}
