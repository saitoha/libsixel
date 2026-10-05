/* SPDX-License-Identifier: MIT */
/* Verify delta byte repeat. */
#include <sixel.h>
int test_delta_byte_repeat(int);

int
test_delta_native_0078(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_repeat(0);
}
