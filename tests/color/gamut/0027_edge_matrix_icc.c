/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/edge_test_common.h"
#include "tests/data/color-numeric/icc.h"

int
test_gamut_0027_edge_matrix_icc(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double result[3];
    int ok;
    int c;
    int i;

    fixture.definition = sixel_cdef_icc(color_icc, sizeof(color_icc));
    ok = fixture.definition != NULL &&
        SIXEL_SUCCEEDED(sixel_allocator_new(&fixture.allocator,
            NULL, NULL, NULL, NULL)) &&
        SIXEL_SUCCEEDED(sixel_gamut_new(&fixture.mapper, fixture.definition,
            SIXEL_COLORSPACE_GAMMA, SIXEL_GAMUT_EDGE_SEEKER,
            fixture.allocator));
    for (i = 0; ok && i < 11; ++i) {
        ok = sixel_gamut_apply(fixture.mapper, edge_origins[i], result,
            SIXEL_COLORSPACE_LINEAR);
        for (c = 0; ok && c < 3; ++c) {
            /* Includes the ICC's 15.16 matrix representation budget. */
            ok = isfinite(result[c]) &&
                fabs(result[c] - edge_expected[1][i][c]) < 3e-4;
        }
    }
    gamut_fixture_free(&fixture);
    (void)argc;
    (void)argv;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
