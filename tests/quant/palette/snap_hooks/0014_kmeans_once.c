/* SPDX-License-Identifier: MIT */
/* Reduced K-means palette executes the requested snap stages exactly once. */
#include "snap_hook_test_common.h"

int
test_snap_hooks_kmeans_once(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return snap_hook_quantizer(0) ? 0 : 1;
}
