/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/execution-map/README.md
 */
#include <stddef.h>
#include <sixel.h>
int lookup_admission_test(int, size_t, SIXELSTATUS, SIXELSTATUS, int, int);

int
test_lookup_force_unsupported_no_fallback(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lookup_admission_test(SIXEL_GPU_POLICY_FORCE, 0,
        SIXEL_FEATURE_ERROR, SIXEL_FEATURE_ERROR, 0, 1);
}
