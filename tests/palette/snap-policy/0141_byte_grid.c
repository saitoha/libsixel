/* SPDX-License-Identifier: MIT */
/* Byte hooks retain bounded integer safe-tone output. */
#include "../../_support/quant/palette/snap_hooks/snap_hook_test_common.h"

int
test_snap_hooks_byte_grid(int argc, char **argv)
{
    double value[3];
    int sample;
    int bound;
    int expected;
    int ok;

    (void)argc;
    (void)argv;
    ok = 1;
    snap_hook_settings(2, 4, 1);
    for (sample = -1; sample <= 256; ++sample) {
        value[0] = value[1] = value[2] = sample;
        bound = sample < 0 ? 0 : sample > 255 ? 255 : sample;
        expected = (((bound * 100 + 127) / 255) * 255 + 50) / 100;
        sixel_palette_snap_triple(value, 1, SIXEL_PIXELFORMAT_RGB888,
            SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER);
        ok = ok && value[0] == expected && value[1] == expected &&
            value[2] == expected;
    }
    sixel_set_palette_snap_override(NULL);
    return ok ? 0 : 1;
}
