/* SPDX-License-Identifier: MIT */
/* Verify delta byte burkes keep. */
#include <sixel.h>
int test_delta_byte_method(int);

int
test_delta_native_0064(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_method(SIXEL_DIFFUSE_BURKES);
}
