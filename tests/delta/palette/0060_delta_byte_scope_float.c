/* SPDX-License-Identifier: MIT */
/* Verify delta byte scope float. */
#include <sixel.h>
int test_delta_byte_scope(int);

int
test_delta_native_0077(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_scope(5);
}
