/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/ray_test_common.h"

int
test_gamut_0045_ray_nonfinite_atomic(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double const input[][3] = {{NAN, 0, 0}, {0, INFINITY, 0},
        {0, 0, -INFINITY}};
    double result[3] = {7, 8, 9};
    double const saved[3] = {7, 8, 9};
    int i;
    int ok;

    ok = ray_fixture_init(&fixture, "srgb");
    for (i = 0; ok && i < 3; ++i) {
        ok = !sixel_gamut_apply(fixture.mapper, input[i], result,
            SIXEL_COLORSPACE_LINEAR) &&
            memcmp(result, saved, sizeof(saved)) == 0;
    }
    gamut_fixture_free(&fixture);
    (void)argc;
    (void)argv;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
