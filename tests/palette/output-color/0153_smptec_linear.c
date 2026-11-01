/* SPDX-License-Identifier: MIT */
/* Verify smptec linear native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(1, 1, 0, 0, NULL);
}
