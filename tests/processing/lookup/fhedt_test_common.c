/* SPDX-License-Identifier: MIT */
/* Discrete-grid references are independent of the separable EDT. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/lookup-fhedt-float32.h"

static int const fhedt_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static double const fhedt_minima[5][3] = {
    {0, 0, 0}, {0, 0, 0}, {0, -.5, -.5},
    {0, -1.5, -1.5}, {0, -1, -1}
};
static double const fhedt_ranges[5][3] = {
    {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 3, 3}, {1, 2, 2}
};

static int
fhedt_setup(sixel_lookup_fhedt_float32_t *lookup, float const *palette,
            int count, int space, int refine, int dist2, int cache, int res)
{
    double range;
    float weight;

    range = fhedt_ranges[space][1];
    weight = (float)(1.0 / (range * range));
    return SIXEL_SUCCEEDED(sixel_lookup_fhedt_float32_configure(lookup,
        palette, count, res, refine, dist2, cache, 1, 8, 8, 0, 0,
        1, weight, weight, fhedt_formats[space], 0));
}

static int
fhedt_expect(sixel_lookup_fhedt_float32_t *lookup, float const *query,
             int expected)
{
    int actual;

    actual = sixel_lookup_fhedt_float32_map(lookup, query);
    if (actual != expected) {
        fprintf(stderr, "query %.9g %.9g %.9g: got %d expected %d\n",
            (double)query[0], (double)query[1], (double)query[2],
            actual, expected);
    }
    return actual == expected;
}

static int
fhedt_grid(sixel_lookup_fhedt_float32_t *lookup, int space, int expanded,
           int resolution)
{
    static int const seeds[9][3] = {
        {0,0,0}, {63,63,63}, {3,19,47}, {41,7,24}, {15,51,9},
        {56,34,13}, {28,46,55}, {9,12,39}, {32,32,32}
    };
    float palette[27];
    float query[3];
    double minimum[3];
    double span[3];
    double distances[9];
    double best;
    double delta;
    double weight;
    int points[9][3];
    int coord[3];
    int index;
    int axis;
    int sample;
    int actual;
    int limit;
    uint32_t state;

    state = 7;
    limit = resolution - 1;
    for (axis = 0; axis < 3; ++axis) {
        minimum[axis] = fhedt_minima[space][axis];
        span[axis] = fhedt_ranges[space][axis];
        if (expanded) {
            minimum[axis] -= axis + 1;
            span[axis] += (axis + 1) * 2;
        }
    }
    for (index = 0; index < 9; ++index) {
        for (axis = 0; axis < 3; ++axis) {
            points[index][axis] = (seeds[index][axis] * limit + 31) / 63;
            palette[index * 3 + axis] = (float)(minimum[axis] +
                span[axis] * points[index][axis] / limit);
        }
    }
    if (!fhedt_setup(lookup, palette, 9, space, 0, 0, 0, resolution)) {
        return 0;
    }
    /* Query lattice sites, not arbitrary continuous colors: FHEDT is
     * approximate between sites and after seed quantization. Index errors
     * are below 1e-4 grid units; every query rounds to the stated integer.
     * Exhaustive distances accept ties plus 2e-6 relative metric rounding,
     * far below the deliberately unequal winning-distance witnesses. */
    for (sample = 0; sample < 701; ++sample) {
        for (axis = 0; axis < 3; ++axis) {
            state = state * 1664525U + 1013904223U;
            coord[axis] = (int)((state >> 16) % (unsigned)resolution);
            query[axis] = (float)(minimum[axis] +
                span[axis] * coord[axis] / limit);
        }
        best = DBL_MAX;
        for (index = 0; index < 9; ++index) {
            distances[index] = 0;
            for (axis = 0; axis < 3; ++axis) {
                delta = coord[axis] - points[index][axis];
                weight = span[axis] / fhedt_ranges[space][axis];
                distances[index] += delta * delta * weight * weight;
            }
            if (distances[index] < best) {
                best = distances[index];
            }
        }
        actual = sixel_lookup_fhedt_float32_map(lookup, query);
        if (actual < 0 || actual >= 9 ||
                distances[actual] > best + 2e-6 * (1 + best)) {
            fprintf(stderr, "grid W=%d expanded=%d q=%d,%d,%d index=%d\n",
                space, expanded, coord[0], coord[1], coord[2], actual);
            return 0;
        }
    }
    return 1;
}

static int
fhedt_value(sixel_lookup_fhedt_float32_t *lookup, char const *mode,
            int space, int setting)
{
    float palette[12] = {0};
    float query[3] = {0};
    float other[12];
    sixel_lookup_fhedt_float32_t *second;
    uint32_t signature;
    int axis;
    int ok;

    if (strcmp(mode, "outside") == 0) {
        palette[3] = palette[4] = 1;
        if (!fhedt_setup(lookup, palette, 2, space, 1, setting, 1, 64)) {
            return 0;
        }
        query[0] = 2;
        ok = fhedt_expect(lookup, query, 1);
        /* These exterior colors clamp to the SAME grid coordinate but
         * their strict native nearest colors differ. Cache must not alias. */
        query[0] = 16;
        query[1] = -16;
        return ok && fhedt_expect(lookup, query, 0);
    }
    if (strcmp(mode, "refine") == 0) {
        /* Distinct adjacent seeds; the original (sub-byte) values decide
         * between them. Query rounds to grid 31; corner 32 must win. */
        palette[0] = .483f;
        palette[3] = .501f;
        query[0] = .494f;
        return fhedt_setup(lookup, palette, 2, space, 1, setting, 0, 64)
            && fhedt_expect(lookup, query, 1);
    }
    if (strcmp(mode, "enclosing") == 0) {
        palette[0] = .483f;
        palette[3] = .524f;
        query[0] = .501f;
        /* Rounded site 32 and site 33 both carry #1. The enclosing
         * site's lower corner 31 carries the native nearest color #0. */
        return fhedt_setup(lookup, palette, 2, space, 1, 0, 0, 64)
            && fhedt_expect(lookup, query, 0);
    }
    if (strcmp(mode, "collision_refine") == 0) {
        palette[0] = .501f;
        palette[3] = .503f;
        query[0] = .501f;
        /* No Voronoi boundary exists with just one occupied seed.
         * Refinement must still inspect both native palette entries. */
        return fhedt_setup(lookup, palette, 2, space, 1, setting, 0, 64)
            && fhedt_expect(lookup, query, 0);
    }
    if (strcmp(mode, "collision") == 0) {
        palette[0] = .501f;
        palette[3] = .503f;
        query[0] = .501f;
        /* Both seeds round to 32. The last seed owns the cell; this
         * deliberately does not claim continuous nearest-color accuracy. */
        return fhedt_setup(lookup, palette, 2, space, 0, 0, 0, 64)
            && fhedt_expect(lookup, query, 1);
    }
    if (strcmp(mode, "extreme") == 0) {
        for (axis = 0; axis < 3; ++axis) {
            palette[axis] = -FLT_MAX;
            palette[3 + axis] = FLT_MAX;
            query[axis] = FLT_MAX * .75f;
        }
        return fhedt_setup(lookup, palette, 2, space, 1, 1, 0, 64)
            && fhedt_expect(lookup, query, 1);
    }
    if (strcmp(mode, "outside_extreme") == 0 ||
            strcmp(mode, "outside_tiny") == 0) {
        for (axis = 0; axis < 3; ++axis) {
            palette[axis] = strcmp(mode, "outside_extreme") == 0
                ? FLT_MAX : (float)ldexp(3, -120);
            palette[3 + axis] = strcmp(mode, "outside_extreme") == 0
                ? FLT_MAX * .75f : (float)ldexp(1, -120);
            query[axis] = strcmp(mode, "outside_extreme") == 0
                ? -FLT_MAX : (float)ldexp(-1, -120);
        }
        /* Both native distances overflow/underflow float arithmetic.
         * Their double-distance ordering has a strict relative margin. */
        return fhedt_setup(lookup, palette, 2, space, 1, 0, 1, 64)
            && fhedt_expect(lookup, query, 1);
    }
    if (strcmp(mode, "isolation") == 0) {
        palette[0] = -1;
        palette[3] = -2;
        memcpy(other, palette, sizeof(other));
        other[0] = -2;
        other[3] = -1;
        query[0] = -1;
        second = NULL;
        if (SIXEL_FAILED(sixel_lookup_fhedt_float32_create(
                lookup->allocator, &second))) {
            return 0;
        }
        ok = fhedt_setup(lookup, palette, 2, space, 1, 0, 1, 64)
            && fhedt_setup(second, other, 2, space, 1, 0, 1, 64)
            && fhedt_expect(lookup, query, 0)
            && fhedt_expect(second, query, 1)
            && fhedt_expect(lookup, query, 0);
        sixel_lookup_fhedt_float32_unref(second);
        return ok;
    }
    if (strcmp(mode, "signature") == 0) {
        palette[0] = -1;
        signature = sixel_lookup_fhedt_float32_signature(palette,
            2, 64, 1, 1, 1, 1, 3, fhedt_formats[space]);
        palette[0] = -2;
        return signature != sixel_lookup_fhedt_float32_signature(palette,
            2, 64, 1, 1, 1, 1, 3, fhedt_formats[space]);
    }
    if (strcmp(mode, "nonfinite") == 0) {
        palette[0] = NAN;
        return sixel_lookup_fhedt_float32_configure(lookup, palette,
            2, 64, 0, 0, 0, 1, 8, 8, 0, 0, 1, 1, 1,
            fhedt_formats[space], 0) == SIXEL_BAD_ARGUMENT;
    }
    /* Rebuilding an object must invalidate cached labels for extended
     * palettes even if the allocator reuses the shared-state address. */
    palette[0] = -1;
    palette[3] = -2;
    query[0] = -1;
    memcpy(other, palette, sizeof(other));
    other[0] = -2;
    other[3] = -1;
    return fhedt_setup(lookup, palette, 2, space, 1, 0, 1, 64)
        && fhedt_expect(lookup, query, 0)
        && fhedt_setup(lookup, other, 2, space, 1, 1, 1, 64)
        && fhedt_expect(lookup, query, 1);
}

int
test_fhedt_common(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_fhedt_float32_t *lookup;
    int ok;
    int space;
    int resolution;

    allocator = NULL;
    lookup = NULL;
    ok = 0;
    if (argc != 5 && argc != 6) {
        return EXIT_FAILURE;
    }
    space = atoi(argv[2]);
    resolution = argc == 6 ? atoi(argv[5]) : 64;
    if (space < 0 || space > 4 ||
            SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_lookup_fhedt_float32_create(allocator,
                &lookup))) {
        goto end;
    }
    ok = strcmp(argv[1], "grid") == 0
        ? fhedt_grid(lookup, space, atoi(argv[3]), resolution)
        : fhedt_value(lookup, argv[1], space, atoi(argv[4]));
end:
    sixel_lookup_fhedt_float32_unref(lookup);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
