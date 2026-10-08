/* SPDX-License-Identifier: MIT */
/* A later invalid coordinate leaves the complete float palette unchanged. */
#include "snap_hook_test_common.h"

int
test_snap_hooks_final_rollback(int argc, char **argv)
{
    float value[6] = {-.25f, 1.25f, .3f, .2f, .4f, NAN};
    float saved[6];
    int ok;

    (void)argc;
    (void)argv;
    memcpy(saved, value, sizeof(saved));
    snap_hook_settings(1, 4, .25);
    sixel_palette_reversible_palette_float(value, 2,
        SIXEL_PIXELFORMAT_RGBFLOAT32);
    ok = memcmp(value, saved, sizeof(saved)) == 0;
    sixel_set_palette_snap_override(NULL);
    return ok ? 0 : 1;
}
