/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/certlut.md
 */
#include <sixel.h>
int gpu_byte_state_test(int, int);

int
test_certlut_gpu_palette_rebuild(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gpu_byte_state_test(SIXEL_LUT_POLICY_CERTLUT, 1);
}
