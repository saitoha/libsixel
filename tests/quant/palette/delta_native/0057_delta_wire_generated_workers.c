/* SPDX-License-Identifier: MIT */
/* Verify delta wire generated workers. */
#include <stddef.h>
int test_delta_wire_generated(int);

int
test_delta_native_0057(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_wire_generated(1);
}
