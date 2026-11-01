/* SPDX-License-Identifier: MIT */
/* Verify delta byte scope or. */
#include <sixel.h>
int test_delta_byte_scope(int);

int
test_delta_native_0073(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_scope(1);
}
