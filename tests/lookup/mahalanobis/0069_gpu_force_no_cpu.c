/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
 */
#include <sixel.h>
int indexed_gpu_lookup_preparation_test(int, int);

int
test_mahalanobis_gpu_force_no_cpu(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_gpu_lookup_preparation_test(0, SIXEL_LUT_POLICY_MAHALANOBIS);
}
