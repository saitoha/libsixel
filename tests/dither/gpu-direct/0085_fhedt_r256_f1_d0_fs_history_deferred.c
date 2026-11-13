/* SPDX-License-Identifier: MIT */
/* Verify fhedt r256 f1 d0 fs history deferred. */
#include <sixel.h>
int test_direct_fhedt_case(int, int, int);

int
test_dither_gpu_direct_0085(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_direct_fhedt_case(3, SIXEL_DIFFUSE_FS, 2);
}
