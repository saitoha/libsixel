/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "ray_test_common.h"

int
test_gamut_0043_ray_identity(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double const pixels[][3] = {
        {1.1, -.02, .02}, {0, 0, 0}, {1, 1, 1}, {.3, .3, .3}
    };
    double result[3];
    int i;
    int ok;

    ok = ray_fixture_init(&fixture, "display-p3");
    for (i = 0; ok && i < 4; ++i) {
        ok = sixel_gamut_apply(fixture.mapper, pixels[i], result,
            SIXEL_COLORSPACE_LINEAR) &&
            memcmp(result, pixels[i], sizeof(result)) == 0;
    }
    gamut_fixture_free(&fixture);
    (void)argc;
    (void)argv;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
