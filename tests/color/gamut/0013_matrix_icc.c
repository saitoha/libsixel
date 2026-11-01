/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_test_common.h"
#include "tests/data/color-numeric/icc.h"

int
test_gamut_0013_matrix_icc(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double result[3];
    int ok;
    int c;

    (void)argc;
    (void)argv;
    fixture.definition = sixel_cdef_icc(color_icc, sizeof(color_icc));
    ok = fixture.definition != NULL &&
        SIXEL_SUCCEEDED(sixel_allocator_new(&fixture.allocator,
            NULL, NULL, NULL, NULL)) &&
        SIXEL_SUCCEEDED(sixel_gamut_new(&fixture.mapper, fixture.definition,
            SIXEL_COLORSPACE_GAMMA, SIXEL_GAMUT_LOCAL_MINDE,
            fixture.allocator)) &&
        sixel_gamut_apply(fixture.mapper, gamut_origins[0], result,
            SIXEL_COLORSPACE_LINEAR);
    for (c = 0; ok && c < 3; ++c) {
        /* ICC 15.16 colorants add their own representation budget. */
        ok = isfinite(result[c]) &&
            fabs(result[c] - gamut_expected[1][0][c]) < 0.0003;
    }
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
