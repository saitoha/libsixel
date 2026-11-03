/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/certlut.md
 */
#include <sixel.h>
int indexed_gpu_lookup_preparation_test(int, int);

int
test_certlut_gpu_auto_no_cpu(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return indexed_gpu_lookup_preparation_test(1, SIXEL_LUT_POLICY_CERTLUT);
}
