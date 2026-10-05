/* SPDX-License-Identifier: MIT */
/* Verify delta byte repeat workers. */
#include <sixel.h>
int test_delta_byte_repeat(int);

int
test_delta_native_0079(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_repeat(1);
}
