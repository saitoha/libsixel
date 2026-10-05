/* SPDX-License-Identifier: MIT */
/* Verify delta wire float palette. */
#include <stddef.h>
int test_delta_wire_palette(int);

int
test_delta_native_0054(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_wire_palette(1);
}
