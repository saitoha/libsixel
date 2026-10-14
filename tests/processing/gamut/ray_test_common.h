/* SPDX-License-Identifier: MIT */
#include "gamut_test_common.h"
#include "tests/data/color-numeric/ray-reference.h"

static inline int
ray_fixture_init(gamut_fixture_t *fixture, char const *name)
{
    fixture->definition = sixel_cdef_name(name);
    return fixture->definition != NULL &&
        SIXEL_SUCCEEDED(sixel_allocator_new(&fixture->allocator,
            NULL, NULL, NULL, NULL)) &&
        SIXEL_SUCCEEDED(sixel_gamut_new(&fixture->mapper,
            fixture->definition, SIXEL_COLORSPACE_GAMMA,
            SIXEL_GAMUT_RAY_TRACE, fixture->allocator));
}

static inline int
ray_reference_check(int space)
{
    char const *names[] = {"srgb", "display-p3", "adobe-rgb", "dci-p3",
        "linear"};
    gamut_fixture_t fixture = {0};
    double result[3];
    int ok;
    int i;
    int c;
    int reference;

    if (space == 4) {
        ok = SIXEL_SUCCEEDED(sixel_allocator_new(&fixture.allocator,
            NULL, NULL, NULL, NULL)) &&
            SIXEL_SUCCEEDED(sixel_gamut_new(&fixture.mapper, NULL,
                SIXEL_COLORSPACE_LINEAR, SIXEL_GAMUT_RAY_TRACE,
                fixture.allocator));
    } else {
        ok = ray_fixture_init(&fixture, names[space]);
    }
    reference = space == 4 ? 0 : space;
    for (i = 0; ok && i < 12; ++i) {
        ok = sixel_gamut_apply(fixture.mapper, ray_origins[i], result,
            SIXEL_COLORSPACE_LINEAR);
        for (c = 0; ok && c < 3; ++c) {
            /* Separate matrix derivation and six-face intersections agree
             * within 3e-5 linear channel units, not bitwise math results. */
            ok = isfinite(result[c]) &&
                fabs(result[c] - ray_expected[reference][i][c]) < 3e-5;
            if (!ok) {
                fprintf(stderr, "%s Ray Trace %d/%d %.12g != %.12g\n",
                    names[space], i, c, result[c],
                    ray_expected[reference][i][c]);
            }
        }
    }
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
