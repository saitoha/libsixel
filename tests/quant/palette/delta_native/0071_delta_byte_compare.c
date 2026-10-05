/* SPDX-License-Identifier: MIT */
/* Verify delta byte compare. */
#include <sixel.h>
int test_delta_byte_compare(int);

int
test_delta_native_0071(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_compare(0);
}
