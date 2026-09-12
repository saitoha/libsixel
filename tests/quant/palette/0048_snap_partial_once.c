/* SPDX-License-Identifier: MIT */
/* A partial final snap moves typed coordinates once and refreshes bytes. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette-common-snap.h"

int
test_palette_0048_snap_partial(int argc, char **argv)
{
    sixel_palette_snap_options_t options;
    float value[3] = { 0.5f, 0.5f, 0.5f };
    unsigned char packed[3] = { 128U, 128U, 128U };
    double expected;
    int channel;
    int valid;

    (void)argc;
    (void)argv;
    memset(&options, 0, sizeof(options));
    options.policy_override = 1;
    options.policy = SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE;
    options.approach_rate_override = 1;
    options.approach_rate = 0.25;
    sixel_set_palette_snap_override(&options);
    sixel_palette_snap_entries(packed, value, 1U,
                               SIXEL_PIXELFORMAT_RGBFLOAT32);
    /* Nearest fixed tone to 0.5 is 128/255; do not round before blending. */
    expected = 0.5 + (128.0 / 255.0 - 0.5) * 0.25;
    valid = !sixel_palette_snap_is_exact();
    for (channel = 0; channel < 3; ++channel) {
        if (fabs((double)value[channel] - expected) > 0.0000001
                || packed[channel] != 128U) {
            valid = 0;
        }
    }
    sixel_set_palette_snap_override(NULL);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
