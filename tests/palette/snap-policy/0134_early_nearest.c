/* SPDX-License-Identifier: MIT */
/* Typed partial snap blends extended coordinates toward independent targets. */
#include "../../_support/quant/palette/snap_hooks/snap_hook_test_common.h"

int
test_snap_hooks_early_nearest(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return snap_hook_targets(1, 0) ? 0 : 1;
}
