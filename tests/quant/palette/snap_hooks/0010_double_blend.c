/* SPDX-License-Identifier: MIT */
/* Active solver snap blends in double precision before palette storage. */
#include "snap_hook_test_common.h"

int
test_snap_hooks_double_blend(int argc, char **argv)
{
    double value[3] = {.123456789012345, .123456789012345, .123456789012345};
    double expected;
    int ok;

    (void)argc;
    (void)argv;
    snap_hook_settings(1, 4, .25);
    expected = .75 * value[0] + .25 * (31.0 / 255);
    sixel_palette_snap_triple(value, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER);
    /* No nonlinear conversion: 1e-14 allows double arithmetic variation,
     * but rejects a float32 store before or after interpolation. */
    ok = isfinite(value[0]) && fabs(value[0] - expected) < 1e-14;
    sixel_set_palette_snap_override(NULL);
    return ok ? 0 : 1;
}
