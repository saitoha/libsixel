/* SPDX-License-Identifier: MIT */
/* Verify smptec cielab native output. */
#include <stddef.h>
int test_output_native(int, int, int, int, char const *);

int
test_output_native_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_output_native(3, 1, 0, 0, NULL);
}
