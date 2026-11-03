/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
 */
#include <sixel.h>
int test_gpu_rectangle(int, int, int);

int
test_mahalanobis_gpu_blue_rectangle_mask(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_rectangle(SIXEL_LUT_POLICY_MAHALANOBIS,
        SIXEL_DIFFUSE_BLUENOISE_DITHER, 1);
}
