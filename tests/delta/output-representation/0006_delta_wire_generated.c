/* SPDX-License-Identifier: MIT */
/* Verify delta wire generated. */
#include <stddef.h>
int test_delta_wire_generated(int);

int
test_delta_native_0056(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_wire_generated(0);
}
