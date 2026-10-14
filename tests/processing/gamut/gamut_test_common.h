/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/gamut.h"
#include "src/colorspace.h"
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "tests/data/color-numeric/gamut-reference.h"

typedef struct gamut_fixture {
    sixel_allocator_t *allocator;
    sixel_color_def_t *definition;
    sixel_gamut_t *mapper;
} gamut_fixture_t;

static inline int
gamut_fixture_init(gamut_fixture_t *fixture, char const *name)
{
    fixture->definition = sixel_cdef_name(name);
    return fixture->definition != NULL &&
        SIXEL_SUCCEEDED(sixel_allocator_new(&fixture->allocator,
            NULL, NULL, NULL, NULL)) &&
        SIXEL_SUCCEEDED(sixel_gamut_new(&fixture->mapper,
            fixture->definition, SIXEL_COLORSPACE_GAMMA,
            fixture->allocator));
}

static inline void
gamut_fixture_free(gamut_fixture_t *fixture)
{
    sixel_gamut_free(fixture->mapper);
    sixel_cdef_free(fixture->definition);
    sixel_allocator_unref(fixture->allocator);
}

static inline int
gamut_reference_check(int space)
{
    char const *names[] = {"srgb", "display-p3", "adobe-rgb", "dci-p3"};
    gamut_fixture_t fixture = {0};
    double result[3];
    int i;
    int c;
    int ok;

    ok = gamut_fixture_init(&fixture, names[space]);
    for (i = 0; ok && i < 4; ++i) {
        ok = sixel_gamut_apply(fixture.mapper, gamut_origins[i], result,
            SIXEL_COLORSPACE_LINEAR);
        for (c = 0; ok && c < 3; ++c) {
            /* Independent primary-matrix/OKLab inverses differ slightly.
             * 3e-5 covers that and the 1e-4 chroma search endpoint, while
             * rejecting raw clipping and the wrong destination gamut. */
            ok = isfinite(result[c]) &&
                fabs(result[c] - gamut_expected[space][i][c]) < 3e-5;
            if (!ok) {
                fprintf(stderr, "%s sample %d channel %d: %.12g != %.12g\n",
                    names[space], i, c, result[c],
                    gamut_expected[space][i][c]);
            }
        }
    }
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
