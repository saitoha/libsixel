/* SPDX-License-Identifier: MIT */
/* Verify delta byte stucki keep. */
#include <sixel.h>
int test_delta_byte_method(int);

int
test_delta_native_0063(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_method(SIXEL_DIFFUSE_STUCKI);
}
