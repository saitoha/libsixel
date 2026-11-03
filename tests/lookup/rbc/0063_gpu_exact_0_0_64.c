/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/rbc.md
 */
#include <sixel.h>
int gpu_lookup_exact_test(int, char **, int);

int
test_rbc_gpu_exact_0_0_64(int argc, char **argv)
{
    char *args[] = { "rbc", "0", "0", "64" };

    (void)argc;
    (void)argv;
    return gpu_lookup_exact_test(4, args, SIXEL_LUT_POLICY_RBC);
}
