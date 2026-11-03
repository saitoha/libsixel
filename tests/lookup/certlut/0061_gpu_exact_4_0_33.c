/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/certlut.md
 */
#include <sixel.h>
int gpu_lookup_exact_test(int, char **, int);

int
test_certlut_gpu_exact_4_0_33(int argc, char **argv)
{
    char *args[] = { "certlut", "4", "0", "33" };

    (void)argc;
    (void)argv;
    return gpu_lookup_exact_test(4, args, SIXEL_LUT_POLICY_CERTLUT);
}
