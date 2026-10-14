/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "edge_test_common.h"

int
test_gamut_0029_edge_bounds(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double lab[3];
    double result[3];
    double repeated[3];
    int ok;
    int h;
    int l;
    int c;

    ok = edge_fixture_init(&fixture, "srgb");
    for (h = 0; ok && h < 360; h += 3) {
        for (l = 1; ok && l < 10; ++l) {
            lab[0] = l * .1;
            lab[1] = .5 * cos(h * 0.017453292519943295);
            lab[2] = .5 * sin(h * 0.017453292519943295);
            ok = sixel_gamut_apply(fixture.mapper, lab, result,
                SIXEL_COLORSPACE_OKLAB) &&
                sixel_color_convert(repeated, result,
                    SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_LINEAR);
            for (c = 0; ok && c < 3; ++c) {
                ok = isfinite(repeated[c]) && repeated[c] >= -2e-7 &&
                    repeated[c] <= 1.0 + 2e-7;
            }
            ok = ok && sixel_gamut_apply(fixture.mapper, result, repeated,
                SIXEL_COLORSPACE_OKLAB) &&
                memcmp(result, repeated, sizeof(result)) == 0;
        }
    }
    gamut_fixture_free(&fixture);
    (void)argc;
    (void)argv;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
