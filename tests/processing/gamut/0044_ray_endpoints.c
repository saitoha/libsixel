/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "ray_test_common.h"

int
test_gamut_0044_ray_endpoints(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double const input[][3] = {{-2, -1, -.5}, {2, 2, 2}};
    double result[3];
    int i;
    int c;
    int ok;

    ok = ray_fixture_init(&fixture, "display-p3");
    for (i = 0; ok && i < 2; ++i) {
        ok = sixel_gamut_apply(fixture.mapper, input[i], result,
            SIXEL_COLORSPACE_LINEAR);
        for (c = 0; ok && c < 3; ++c) {
            ok = isfinite(result[c]) && fabs(result[c] - i) < 3e-6;
        }
    }
    gamut_fixture_free(&fixture);
    (void)argc;
    (void)argv;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
