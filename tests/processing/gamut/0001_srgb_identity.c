/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "gamut_test_common.h"

int
test_gamut_0001_srgb_identity(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double source[3] = {0.125, 0.5, 0.75};
    double result[3];
    int ok;

    (void)argc;
    (void)argv;
    ok = gamut_fixture_init(&fixture, "srgb") &&
        sixel_gamut_apply(fixture.mapper, source, result,
            SIXEL_COLORSPACE_LINEAR) &&
        memcmp(source, result, sizeof(source)) == 0;
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
