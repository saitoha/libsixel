/* SPDX-License-Identifier: MIT */
/* Verify linear heckbert native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0013(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(0, 0, 3, 0, NULL);
}
