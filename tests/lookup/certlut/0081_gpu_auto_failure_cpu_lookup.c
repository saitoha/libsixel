/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/palette-application/lookup-policy/certlut.md
 */
#include <sixel.h>
int certlut_gpu_auto_fallback_test(void);

int
test_certlut_gpu_auto_failure_cpu_lookup(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return certlut_gpu_auto_fallback_test();
}
