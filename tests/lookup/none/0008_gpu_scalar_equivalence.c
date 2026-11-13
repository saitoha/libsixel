/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int gpu_lookup_interleaved(int policy, int scalar);

int
test_none_gpu_scalar_equivalence(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gpu_lookup_interleaved(SIXEL_LUT_POLICY_NONE, 1);
}
