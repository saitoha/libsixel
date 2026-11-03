/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
 */
#include <sixel.h>
int gpu_lookup_exact_test(int, char **, int);

int
test_mahalanobis_gpu_exact_2_0_33(int argc, char **argv)
{
    char *args[] = { "mahalanobis", "2", "0", "33" };

    (void)argc;
    (void)argv;
    return gpu_lookup_exact_test(4, args, SIXEL_LUT_POLICY_MAHALANOBIS);
}
