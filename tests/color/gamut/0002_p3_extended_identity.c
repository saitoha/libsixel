/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_test_common.h"

int
test_gamut_0002_p3_extended_identity(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double source[3] = {1.10, -0.02, 0.02};
    double result[3];
    int ok;

    (void)argc;
    (void)argv;
    /* sRGB-external coordinates strictly inside Display P3. */
    ok = gamut_fixture_init(&fixture, "display-p3") &&
        sixel_gamut_apply(fixture.mapper, source, result,
            SIXEL_COLORSPACE_LINEAR) &&
        memcmp(source, result, sizeof(source)) == 0;
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
