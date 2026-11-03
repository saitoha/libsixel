/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/rbc.md
 */
#include <sixel.h>
int gpu_byte_state_test(int, int);

int
test_rbc_gpu_failed_build_recovery(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gpu_byte_state_test(SIXEL_LUT_POLICY_RBC, 3);
}
