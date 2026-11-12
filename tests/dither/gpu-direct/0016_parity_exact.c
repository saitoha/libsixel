/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int test_gpu_direct_parity_exact(int ncolors);

int
test_gpu_direct_0016_parity_exact(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_direct_parity_exact(256);
}
