/* SPDX-License-Identifier: MIT */
/* Verify smptec auto native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0018(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(0, 1, 4, 0, NULL);
}
