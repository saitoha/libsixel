/* SPDX-License-Identifier: MIT */
/* Typed partial snap blends extended coordinates toward independent targets. */
#include "snap_hook_test_common.h"

int
test_snap_hooks_early_reversible(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return snap_hook_targets(2, 0) ? 0 : 1;
}
