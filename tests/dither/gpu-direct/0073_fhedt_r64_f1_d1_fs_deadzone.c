/* SPDX-License-Identifier: MIT */
/* Verify fhedt r64 f1 d1 fs deadzone. */
#include <sixel.h>
int test_direct_fhedt_case(int, int, int);

int
test_dither_gpu_direct_0073(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_fhedt_case(1, SIXEL_DIFFUSE_FS, 0);
}
