/* SPDX-License-Identifier: MIT */
/* ICC.1:2022 Table 68 orders the type-4 parameters as g,a,b,c,d,e,f. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "src/icc-apply.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/vectors.h"

int test_icc_0009(int argc, char **argv)
{
    sixel_icc_profile_t profile;
    double actual[3];
    size_t i;
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    memset(&profile, 0, sizeof(profile));
    if (!sixel_icc_parse_profile(color_icc, sizeof(color_icc), &profile)) {
        return EXIT_FAILURE;
    }
    ok = 1;
    /* Test nominal device samples; matrix/TRC ICC tags use bounded RGB. */
    for (i = 0; i < 7; ++i) {
        if (!sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(actual,
                color_vectors[i], 3u, &profile, 1u, 1)) {
            ok = 0;
            break;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(actual[channel]) || fabs(actual[channel] -
                    color_vectors[i][6 + channel]) > 2e-4) {
                ok = 0;
            }
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
