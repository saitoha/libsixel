/* SPDX-License-Identifier: MIT */
/* Verify delta byte sierra2 keep. */
#include <sixel.h>
int test_delta_byte_method(int);

int
test_delta_native_0066(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_byte_method(SIXEL_DIFFUSE_SIERRA2);
}
