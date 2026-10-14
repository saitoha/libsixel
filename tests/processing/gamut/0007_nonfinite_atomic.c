/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "gamut_test_common.h"

int
test_gamut_0007_nonfinite_atomic(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double source[3] = {NAN, 0.2, 0.3};
    double result[3] = {7.0, 8.0, 9.0};
    double saved[3];
    int ok;

    (void)argc;
    (void)argv;
    memcpy(saved, result, sizeof(saved));
    ok = gamut_fixture_init(&fixture, "srgb") &&
        !sixel_gamut_apply(fixture.mapper, source, result,
            SIXEL_COLORSPACE_LINEAR) &&
        memcmp(saved, result, sizeof(saved)) == 0;
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
