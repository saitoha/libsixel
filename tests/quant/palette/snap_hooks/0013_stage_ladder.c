/* SPDX-License-Identifier: MIT */
/* Each timing value enables only its additive solver stages. */
#include "snap_hook_test_common.h"

int
test_snap_hooks_stage_ladder(int argc, char **argv)
{
    double value[3];
    double source[3] = {-.25, 1.25, .38480392098426819};
    double expected;
    int stage;
    int timing;
    int ok;

    (void)argc;
    (void)argv;
    ok = 1;
    for (timing = 0; timing <= 4; ++timing) {
        snap_hook_settings(1, timing, .25);
        for (stage = 0; stage <= 4; ++stage) {
            memcpy(value, source, sizeof(value));
            sixel_palette_snap_triple(value, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
                (enum sixel_palette_snap_stage)stage);
            if (stage <= timing) {
                expected = .75 * source[2] + .25 * (99.0 / 255);
                ok = ok && value[0] == -.1875 && value[1] == 1.1875 &&
                    isfinite(value[2]) && fabs(value[2] - expected) < 1e-14;
            } else {
                ok = ok && memcmp(value, source, sizeof(value)) == 0;
            }
        }
    }
    sixel_set_palette_snap_override(NULL);
    return ok ? 0 : 1;
}
