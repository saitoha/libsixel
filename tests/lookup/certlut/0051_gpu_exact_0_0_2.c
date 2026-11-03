/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/certlut.md
 */
#include <sixel.h>
int gpu_lookup_exact_test(int, char **, int);

int
test_certlut_gpu_exact_0_0_2(int argc, char **argv)
{
    char *args[] = { "certlut", "0", "0", "2" };

    (void)argc;
    (void)argv;
    return gpu_lookup_exact_test(4, args, SIXEL_LUT_POLICY_CERTLUT);
}
