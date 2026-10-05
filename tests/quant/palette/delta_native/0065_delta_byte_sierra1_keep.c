/* SPDX-License-Identifier: MIT */
/* Verify delta byte sierra1 keep. */
#include <sixel.h>
int test_delta_byte_method(int);

int
test_delta_native_0065(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_method(SIXEL_DIFFUSE_SIERRA1);
}
