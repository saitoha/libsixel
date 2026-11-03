/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/execution-map/README.md
 */
#include <stddef.h>
#include <sixel.h>
int lookup_admission_test(int, size_t, SIXELSTATUS, SIXELSTATUS, int, int);

int
test_lookup_cutoff_cpu_only(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lookup_admission_test(SIXEL_GPU_POLICY_AUTO, 2,
        SIXEL_RUNTIME_ERROR, SIXEL_BAD_INPUT, 1, 0);
}
