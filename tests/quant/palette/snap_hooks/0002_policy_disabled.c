/* SPDX-License-Identifier: MIT */
/* Inactive typed snap preserves original double coordinates. */
#include "snap_hook_test_common.h"

int
test_snap_hooks_policy_disabled(int argc, char **argv)
{
    double value[3] = {-.25, 1.25, .123456789012345};
    double saved[3];
    int ok;

    (void)argc;
    (void)argv;
    memcpy(saved, value, sizeof(saved));
    snap_hook_settings(0, 4, 1);
    sixel_palette_snap_triple(value, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER);
    ok = memcmp(value, saved, sizeof(saved)) == 0;
    sixel_set_palette_snap_override(NULL);
    return ok ? 0 : 1;
}
