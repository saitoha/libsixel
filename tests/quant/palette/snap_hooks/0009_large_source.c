/* SPDX-License-Identifier: MIT */
/* Full snap preserves target selection beside a huge source component. */
#include "snap_hook_test_common.h"

int
test_snap_hooks_large_source(int argc, char **argv)
{
    double value[3] = {-FLT_MAX, 98.125 / 255.0, 0};
    int ok;

    (void)argc;
    (void)argv;
    snap_hook_settings(1, 4, 1);
    sixel_palette_snap_triple(value, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PALETTE_SNAP_STAGE_INITIAL_SEED);
    ok = value[0] == 0 && fabs(value[1] - 99.0 / 255) < 1e-14 && value[2] == 0;
    sixel_set_palette_snap_override(NULL);
    return ok ? 0 : 1;
}
