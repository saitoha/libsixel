/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/certlut.md
 */
#include <sixel.h>
int certlut_gpu_fixture_test(int);

int
test_certlut_gpu_candidate_overflow(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return certlut_gpu_fixture_test(1);
}
