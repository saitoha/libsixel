/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/certlut.md
 */
#include <sixel.h>
int test_gpu_rectangle(int, int, int);

int
test_certlut_gpu_blue_rectangle_indices(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_gpu_rectangle(SIXEL_LUT_POLICY_CERTLUT,
        SIXEL_DIFFUSE_BLUENOISE_DITHER, 0);
}
