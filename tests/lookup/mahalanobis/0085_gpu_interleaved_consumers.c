/* SPDX-License-Identifier: MIT */
#include <sixel.h>

int gpu_lookup_interleaved(int policy, int scalar);

int
test_mahalanobis_gpu_interleaved_consumers(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gpu_lookup_interleaved(SIXEL_LUT_POLICY_MAHALANOBIS, 0);
}
