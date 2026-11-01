/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/ray_test_common.h"

int
test_gamut_0059_ray_convergence(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    int const spaces[] = {SIXEL_COLORSPACE_LINEAR,
        SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_OKLAB};
    double input[3];
    double mapped[3];
    double result[3];
    int ok;
    int i;
    int s;
    int c;

    (void)argc;
    (void)argv;
    ok = ray_fixture_init(&fixture, "srgb");
    for (i = 0; ok && i < 12; ++i) {
        for (s = 0; ok && s < 3; ++s) {
            ok = sixel_color_convert(input, ray_convergence_origins[i],
                SIXEL_COLORSPACE_LINEAR, spaces[s]);
            /* Exercise the frame's float storage as well as the mapper's
             * double arithmetic. Boundary behavior must not depend on
             * which workspace supplied the color. */
            for (c = 0; c < 3; ++c) {
                input[c] = (float)input[c];
            }
            ok = ok && sixel_gamut_apply(fixture.mapper, input, mapped,
                spaces[s]) && sixel_color_convert(result, mapped,
                spaces[s], SIXEL_COLORSPACE_LINEAR);
            for (c = 0; ok && c < 3; ++c) {
                /* Independently bisected constant-L/h boundary values.
                 * 3e-5 linear units allows the four-ray approximation,
                 * rounded matrix pair and float workspace storage on this
                 * near-converged patch; an opposite-face hit misses by 1. */
                ok = isfinite(result[c]) && fabs(result[c] -
                    ray_convergence_expected[i][c]) <= 3e-5;
                if (!ok) {
                    fprintf(stderr, "convergence %d/%d/%d: %.12g != %.12g\n",
                        i, s, c, result[c], ray_convergence_expected[i][c]);
                }
            }
        }
    }
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
